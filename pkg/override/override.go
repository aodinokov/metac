package override

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"strings"

	"github.com/mitchellh/copystructure"
	"gopkg.in/yaml.v3"
)

// converts string representations to the override
func New(override string) (*map[string]interface{}, error) {
	overrideUnstr := &map[string]interface{}{}

	if strings.HasPrefix(override, "--file:") {
		bOverride, err := ioutil.ReadFile(strings.TrimPrefix(override, "--file:"))
		if err != nil {
			return nil, err
		}
		override = string(bOverride)
	}
	// json must start from '{' in our case
	if strings.HasPrefix(strings.TrimSpace(override), "{") {
		err := json.Unmarshal([]byte(override), overrideUnstr)
		if err != nil {
			return nil, fmt.Errorf("couldn't unmarshal json override %s: %v", override, err)
		}
		return overrideUnstr, nil
	}
	// yaml by default
	err := yaml.Unmarshal([]byte(override), overrideUnstr)
	if err != nil {
		return nil, fmt.Errorf("couldn't unmarshal yaml override %s: %v", override, err)
	}
	return overrideUnstr, nil
}

func MergeAll(in *map[string]interface{}, overrides []string) (*map[string]interface{}, error) {
	if in == nil {
		return nil, fmt.Errorf("module spec isn't loaded")
	}
	for _, ovrd := range overrides {
		overrideUnstr, err := New(ovrd)
		if err != nil {
			return nil, err
		}
		// support only merge so far
		in, _ = Merge(in, overrideUnstr)
	}
	return in, nil
}

// external function that merges maps
// it substututes arrays and leaves (like helm does, see https://github.com/helm/helm/blob/main/pkg/chartutil/coalesce.go#L92)
func Merge(in *map[string]interface{}, override *map[string]interface{}) (*map[string]interface{}, error) {
	//return in, nil
	merge := true
	v := *override

	// Using c.Values directly when coalescing a table can cause problems where
	// the original c.Values is altered. Creating a deep copy stops the problem.
	// This section is fault-tolerant as there is no ability to return an error.

	valuesCopy, err := copystructure.Copy(*in)
	var vc map[string]interface{}
	var ok bool
	if err != nil {
		// If there is an error something is wrong with copying c.Values it
		// means there is a problem in the deep copying package or something
		// wrong with c.Values. In this case we will use c.Values and report
		// an error.
		//printf("warning: unable to copy values, err: %s", err)
		//vc = *in
		return nil, fmt.Errorf("unable to copy values, err: %s", err)
	} else {
		vc, ok = valuesCopy.(map[string]interface{})
		if !ok {
			// c.Values has a map[string]interface{} structure. If the copy of
			// it cannot be treated as map[string]interface{} there is something
			// strangely wrong. Log it and use c.Values
			//log.Printf("warning: unable to convert values copy to values type")
			//vc = *in
			return nil, fmt.Errorf("unable to convert values copy to values type")
		}
	}

	for key, val := range vc {
		if value, ok := v[key]; ok {
			if value == nil && !merge {
				// When the YAML value is null and we are coalescing instead of
				// merging, we remove the value's key.
				// This allows Helm's various sources of values (value files or --set) to
				// remove incompatible keys from any previous chart, file, or set values.
				delete(v, key)
			} else if dest, ok := value.(map[string]interface{}); ok {
				// if v[key] is a table, merge nv's val table into v[key].
				src, ok := val.(map[string]interface{})
				if !ok {
					// If the original value is nil, there is nothing to coalesce, so we don't print
					// the warning
					if val != nil {
						log.Printf("warning: skipped value for %s: Not a table.", key)
					}
				} else {
					// Because v has higher precedence than nv, dest values override src
					// values.
					mergeTables(dest, src, merge)
				}
			}
		} else {
			// If the key is not in v, copy it from nv.
			v[key] = val
		}
	}
	return &v, nil
}

// istable is a special-purpose function to see if the present thing matches the definition of a YAML table.
func istable(v interface{}) bool {
	_, ok := v.(map[string]interface{})
	return ok
}

// coalesceTablesFullKey merges a source map into a destination map.
//
// dest is considered authoritative.
func mergeTables(dst, src map[string]interface{}, merge bool) map[string]interface{} {
	// When --reuse-values is set but there are no modifications yet, return new values
	if src == nil {
		return dst
	}
	if dst == nil {
		return src
	}
	// Because dest has higher precedence than src, dest values override src
	// values.
	for key, val := range src {
		if dv, ok := dst[key]; ok && !merge && dv == nil {
			delete(dst, key)
		} else if !ok {
			dst[key] = val
		} else if istable(val) {
			if istable(dv) {
				mergeTables(dv.(map[string]interface{}), val.(map[string]interface{}), merge)
			} else {
				log.Printf("warning: cannot overwrite table with non table for %s (%v)", key, val)
			}
		} else if istable(dv) && val != nil {
			log.Printf("warning: destination for %s is a table. Ignoring non-table value (%v)", key, val)
		}
	}
	return dst
}
