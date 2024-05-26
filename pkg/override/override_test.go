package override

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"gopkg.in/yaml.v3"
)

func TestApplyOverride(t *testing.T) {
	tcs := []struct {
		in             string
		override       []string
		expectedResult string
		expectedErr    bool
	}{
		{
			in: `
data:
  value1: 1
  value2: 2
  arr:
    - 1
    - 2
    - 3
`,
			override: []string{`
data:
  value1: overriden
  arr:
    - 4
    - 5
newdata: x
`},
			expectedResult: `data:
    arr:
        - 4
        - 5
    value1: overriden
    value2: 2
newdata: x
`,
			expectedErr: false,
		},
		{
			in: `
data:
  value1: 1
  value2: 2
  arr:
    - 1
    - 2
    - 3
`,
			override: []string{"--file:./testdata/test01.yaml"},
			expectedResult: `data:
    arr:
        - 4
        - 5
    value1: overriden
    value2: 2
newdata: x
`,
			expectedErr: false,
		},
		{
			in: `
data:
  value1: 1
  value2: 2
  arr:
    - 1
    - 2
    - 3
`,
			override: []string{`{"data": {"value1": "overriden", "arr": [4,5]}, "newdata": "x"}`},
			expectedResult: `data:
    arr:
        - 4
        - 5
    value1: overriden
    value2: 2
newdata: x
`,
			expectedErr: false,
		},
		{
			in: `
data:
  value1: 1
  value2: 2
  arr:
    - 1
    - 2
    - 3
`,
			override: []string{"--file:./testdata/test01.json"},
			expectedResult: `data:
    arr:
        - 4
        - 5
    value1: overriden
    value2: 2
newdata: x
`,
			expectedErr: false,
		},
	}
	for i, tc := range tcs {
		t.Run(fmt.Sprintf("tc %d", i), func(t *testing.T) {
			var in map[string]interface{}
			err := yaml.Unmarshal([]byte(tc.in), &in)
			if err != nil {
				t.Errorf("coun't unmarshal in %v", err)
			}

			result, err := MergeAll(&in, tc.override)
			if (err != nil) != tc.expectedErr {
				t.Errorf("unexpected err: %v, expected %v", err, tc.expectedErr)
			}
			yResult, err := yaml.Marshal(result)
			if err != nil {
				t.Errorf("couldn't marshal %v", result)
			}
			assert.Equal(t, tc.expectedResult, string(yResult))
		})
	}
}
