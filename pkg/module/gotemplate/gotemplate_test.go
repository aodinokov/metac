package gotemplate

import (
	"fmt"
	"testing"

	"github.com/aodinokov/metac/pkg/module"
	"github.com/stretchr/testify/assert"
)

func TestGoTemplateModule(t *testing.T) {
	runner := module.Runner{
		Paths: []string{"./testdata"},
		Kinds: map[string]module.Kind{"goTemplate": DefaultGoTemplateFactory()},
	}

	tcs := []struct {
		moduleName     string
		specFilename   string
		overrides      []string
		expectedOutput string
		expectedErr    bool
	}{
		{
			moduleName:     "01-minimal",
			expectedOutput: "json: {\n  \"key1\": \"val1\"\n}",
		},
		{
			moduleName:     "01-minimal",
			overrides:      []string{"key1: val2"},
			expectedOutput: "json: {\n  \"key1\": \"val2\"\n}",
		},
		{
			moduleName:  "02-purelib",
			overrides:   []string{"key1: val2"},
			expectedErr: true,
		},
		{
			moduleName:     "03-localfn",
			overrides:      []string{"key1: val2"},
			expectedOutput: "json: {\n  \"key1\": \"val2\"\n}json: {\n  \"key1\": \"val2\"\n}",
		},
	}

	for i, tc := range tcs {
		t.Run(fmt.Sprintf("tc %d", i), func(t *testing.T) {
			instance, err := runner.NewInstance(tc.moduleName, tc.specFilename)
			assert.NoError(t, err)

			output, err := instance.Run("", tc.overrides)
			if (err != nil) != tc.expectedErr {
				t.Errorf("unexpected err: %v, expected %v", err, tc.expectedErr)
			} else {
				assert.Equal(t, tc.expectedOutput, string(output))
			}
		})
	}

}
