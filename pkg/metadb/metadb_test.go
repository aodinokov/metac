package metadb

import (
	"fmt"
	"io/ioutil"
	"log"
	"testing"

	"github.com/aodinokov/metac/pkg/dwarfy"
	"gopkg.in/yaml.v3"
)

func TestNewMetaDb(t *testing.T) {
	tcs := []struct {
		inYamlPath string
		err        bool
	}{
		{
			inYamlPath: "testdata/c_app.yaml",
			err:        false,
		},
		{
			inYamlPath: "testdata/cpp_app.yaml",
			err:        false,
		},
		{
			inYamlPath: "testdata/c_app_complex_gcc.yaml",
			err:        false,
		},
		{
			inYamlPath: "testdata/c_app_complex_clang.yaml",
			err:        false,
		},
	}

	for _, tc := range tcs {
		t.Run(fmt.Sprintf("tc %s", tc.inYamlPath), func(t *testing.T) {
			buf, err := ioutil.ReadFile(tc.inYamlPath)
			if err != nil {
				t.Errorf("could not read yaml %s: %v", tc.inYamlPath, err)
			}
			data, err := dwarfy.NewDataFromYamlBuffer(buf)
			if err != nil {
				t.Errorf("could not parse yaml %s: %v", tc.inYamlPath, err)
			}
			db, err := NewMetaDb(data)
			if (err != nil) != tc.err {
				t.Errorf("expected error %v, got otherwise: %v", tc.err, err)
			}
			buf, err = yaml.Marshal(&db)
			if err == nil {
				log.Printf("%s", string(buf))
			}
		})
	}
}
