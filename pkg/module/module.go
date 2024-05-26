package module

import (
	"fmt"
	"io/ioutil"
	"log"
	"path/filepath"
	"strings"

	"gopkg.in/yaml.v3"
)

const DefaultSpecRootName = "modules"
const SpecFilename = "module.yaml"

type SpecHeader struct {
	ApiVersion string `json:"apiVersion" yaml:"apiVersion"` // some version
	Kind       string // goTemplate, starlark, etc

	Metadata struct {
		Name string // module name
	}
}

func GetSpecHeader(buf []byte) (*SpecHeader, error) {
	spec := SpecHeader{}
	err := yaml.Unmarshal(buf, &spec)
	if err != nil {
		return nil, err
	}
	return &spec, nil
}

type SpecUnstructured struct {
	SpecHeader `yaml:",inline"`
	Spec       *map[string]interface{} `json:",omitempty" yaml:",omitempty"`
}

/*
  how to use
  should be var with default Kinds: goTemplate, Starlark
  but it must be possible to create our own

  interface ModuleKind {
	...
  }

  kinds :=  map[string]ModuleKind {
	"GoTemplate": &GoTemplateModuleKind{},
	"Starlark": &StarlarkExecutor{}, // or use NewStarlarkModuleKind() with modifiers
  }

  moduleRunner := NewModuleRunnner(kinds, and Paths)

  + ...put here all params - all -M go to Paths,
	-m to name, -V - to overrides

  out, err := module.Run()

  we must expose a function moduleRun that would
  create a copy of the current moduleRunner Paths,
  and execute module with other overrides



*/

type Instance interface {
	Run(workDir string, overrides []string) ([]byte, error)
}

type Kind interface {
	NewInstance(runner *Runner, path string, specFilename string) (Instance, error)
}

type Runner struct {
	Kinds map[string]Kind
	Paths []string // lookup paths
}

func DefaultPaths() []string {
	return []string{filepath.FromSlash("./" + DefaultSpecRootName)}
}

func nameIsLegal(name string) bool {
	if strings.Contains(name, "/") ||
		strings.Contains(name, "\\") {
		return true
	}
	if name == ".." ||
		name == "." {
		return true
	}

	return false
}

func (mr *Runner) GetSpecHeader(moduleYamlPath string) (*SpecHeader, error) {
	buf, err := ioutil.ReadFile(moduleYamlPath)
	if err != nil {
		return nil, err
	}
	spec, err := GetSpecHeader(buf)
	if err != nil {
		return nil, fmt.Errorf("found %s, but couldn't parse that: %v", moduleYamlPath, err)
	}
	return spec, nil
}

func (mr *Runner) InfoByName(name string) ([]*struct {
	Path string
	Kind string
}, error) {
	if nameIsLegal(name) {
		return nil, fmt.Errorf("name %s is illigal", name)
	}
	var modulePaths []*struct {
		Path string
		Kind string
	}
	for _, path := range mr.Paths {
		modulePath := filepath.FromSlash(filepath.ToSlash(path) + "/" + name)
		moduleYamlPath := filepath.FromSlash(filepath.ToSlash(modulePath) + "/" + SpecFilename)
		spec, err := mr.GetSpecHeader(moduleYamlPath)
		if err != nil {
			log.Printf("%v trying next path", err)
			continue
		}
		kind, ok := mr.Kinds[spec.Kind]
		if !ok {
			log.Printf("found %s, but couldn't find kind %s.. trying next path", modulePath, kind)
			continue
		}
		modulePaths = append(modulePaths, &struct {
			Path string
			Kind string
		}{Path: modulePath, Kind: spec.Kind})
	}
	return modulePaths, nil
}

func (mr *Runner) newInstanceByFullInfo(kindName string, path string, specFilename string) (Instance, error) {
	kind, ok := mr.Kinds[kindName]
	if !ok {
		return nil, fmt.Errorf("couldn't find kind %s", kind)
	}
	return kind.NewInstance(mr, path, specFilename)
}

// just return the first successful
func (mr *Runner) NewInstance(name string, specFilename string) (Instance, error) {
	/*
		cases:
		1. name non-empty, specFilename isn't set (will use SpecFilename == module.yaml)
		2. name non-empty, specFilename non-empty - find specFilename in module and run it
		3. name isn't set, specFilename non-empty - the module is located in the same folder as specFilename

	*/
	if name == "" { // this means we want to run the module by the specfile path
		spec, err := mr.GetSpecHeader(specFilename)
		if err != nil {
			return nil, err
		}
		path := filepath.FromSlash(filepath.Dir(filepath.ToSlash(specFilename)))
		specFilename = filepath.FromSlash(filepath.Base(filepath.ToSlash(specFilename)))
		return mr.newInstanceByFullInfo(spec.Kind, path, specFilename)
	} else { // try to find module in -M dirs
		if nameIsLegal(name) {
			return nil, fmt.Errorf("name %s is illigal", name)
		}

		if specFilename == "" {
			specFilename = SpecFilename
		}

		infos, err := mr.InfoByName(name)
		if err != nil {
			return nil, err
		}
		if len(infos) == 0 {
			return nil, fmt.Errorf("wasn't able to find module %s", name)
		}

		for _, info := range infos {
			instance, err := mr.newInstanceByFullInfo(info.Kind, info.Path, specFilename)
			if err != nil {
				log.Printf("error during loading %s, %s, %s", name, info.Path, info.Kind)
				continue
			}
			return instance, nil
		}
	}
	return nil, fmt.Errorf("wasn't able to load a proper module %s specFilename %s", name, specFilename)
}
