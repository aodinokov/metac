package module

import (
	"fmt"
	"testing"

	"gopkg.in/yaml.v3"
)

type Marshallable interface {
	Marshal() ([]byte, error)
}

type ExampleModuleSpec struct {
	SpecHeader `yaml:",inline"`
	Spec       struct {
		Test string
	}
}

func (d *ExampleModuleSpec) Marshal() ([]byte, error) { return yaml.Marshal(d) }

func TestGetModuleSpecHeader(t *testing.T) {
	tcs := []struct {
		in              Marshallable
		pass            bool
		expectedVersion string
	}{
		{
			in: &ExampleModuleSpec{
				SpecHeader: SpecHeader{
					ApiVersion: "github.com/aodinokov/metac/v1",
					Kind:       "exampleModule",
					Metadata: struct{ Name string }{
						Name: "name",
					},
				},
				Spec: struct{ Test string }{
					Test: "test",
				},
			},
			pass:            true,
			expectedVersion: "github.com/aodinokov/metac/v1",
		},
	}
	for i, tc := range tcs {
		t.Run(fmt.Sprintf("tc %d", i), func(t *testing.T) {
			buf, err := tc.in.Marshal()
			if err != nil {
				t.Errorf("marshal err %v", err)
			}
			spec, err := GetSpecHeader(buf)
			if (err == nil) != tc.pass {
				t.Errorf("expected pass %v and got %v", tc.pass, err == nil)
			}
			if spec.ApiVersion != tc.expectedVersion {
				t.Errorf("expected pass %s and got %s", tc.expectedVersion, spec.ApiVersion)
			}
		})
	}
}

type ExampleInstance struct {
}

func (i *ExampleInstance) Run(workDir string, overrides []string) ([]byte, error) { return nil, nil }

type ExampleKindFactory struct {
	instance *ExampleInstance
}

func (f *ExampleKindFactory) NewInstance(runner *Runner, path string, specFilename string) (Instance, error) {
	return f.instance, nil
}

func TestInstantination(t *testing.T) {
	tcs := []struct {
		name         string
		specFilename string
		pass         bool
	}{
		{
			name: "example01",
			pass: true,
		},
		{
			specFilename: "./testdata/example01/module.yaml",
			pass:         true,
		},
		{
			name: "nonexistent",
			pass: false,
		},
		{
			// cornercase1 - empty name - must fail
			name: "",
			pass: false,
		},
		{
			// cornercase2 - name with slashes - must fail
			name: "../testdata/example01",
			pass: false,
		},
	}

	// lets re-use Runner
	exampleInstace := &ExampleInstance{}
	runner := Runner{
		Paths: []string{"./testdata"},
		Kinds: map[string]Kind{"exampleKind": &ExampleKindFactory{instance: exampleInstace}},
	}

	for i, tc := range tcs {
		t.Run(fmt.Sprintf("tc %d", i), func(t *testing.T) {
			instance, err := runner.NewInstance(tc.name, tc.specFilename)
			if (err == nil) != tc.pass {
				t.Errorf("failure %v doesn't correlate with expected %v", err, tc.pass)
			}
			if tc.pass && instance != exampleInstace {
				t.Errorf("incorrect instance %v expected %v", instance, exampleInstace)
			}
		})
	}
}
