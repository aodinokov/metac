package funcs

import (
	"debug/elf"
	"debug/macho"
	"debug/pe"
	"fmt"
	"io/ioutil"
	"log"
	"text/template"

	"github.com/aodinokov/metac/pkg/dwarfy"
	"github.com/aodinokov/metac/pkg/metadb"
)

// list of exported functions
var genericMap = map[string]interface{}{
	//dwarf
	"dwarfFromElf": func(filename string) (*dwarfy.Data, error) {
		elfFile, err := elf.Open(filename)
		if err != nil {
			return nil, fmt.Errorf("could not open elf %s: %v", filename, err)
		}
		defer elfFile.Close()
		dwarfData, err := elfFile.DWARF()
		if err != nil {
			return nil, fmt.Errorf("could not get DWARF from %s: %v", filename, err)
		}
		return dwarfy.NewData(dwarfData)
	},
	"dwarfFromPE": func(filename string) (*dwarfy.Data, error) {
		peFile, err := pe.Open(filename)
		if err != nil {
			return nil, fmt.Errorf("could not open pe %s: %v", filename, err)
		}
		defer peFile.Close()
		dwarfData, err := peFile.DWARF()
		if err != nil {
			return nil, fmt.Errorf("could not get DWARF from %s: %v", filename, err)
		}
		return dwarfy.NewData(dwarfData)
	},
	"dwarfFromMacho": func(filename string) (*dwarfy.Data, error) {
		machoFile, err := macho.Open(filename)
		if err != nil {
			return nil, fmt.Errorf("could not open macho %s: %v", filename, err)
		}
		defer machoFile.Close()
		dwarfData, err := machoFile.DWARF()
		if err != nil {
			return nil, fmt.Errorf("could not get DWARF from %s: %v", filename, err)
		}
		return dwarfy.NewData(dwarfData)
	},
	"dwarfFromYamlFile": func(filename string) (*dwarfy.Data, error) {
		buf, err := ioutil.ReadFile(filename)
		if err != nil {
			return nil, fmt.Errorf("could not read yaml %s: %v", filename, err)
		}
		return dwarfy.NewDataFromYamlBuffer(buf)
	},
	"dwarfFromJsonFile": func(filename string) (*dwarfy.Data, error) {
		buf, err := ioutil.ReadFile(filename)
		if err != nil {
			return nil, fmt.Errorf("could not read json %s: %v", filename, err)
		}
		return dwarfy.NewDataFromJsonBuffer(buf)
	},
	"dwarfFromYaml":         dwarfy.NewDataFromYamlBuffer,
	"dwarfFromJson":         dwarfy.NewDataFromJsonBuffer,
	"dwarfFromUnstructured": dwarfy.NewDataFromUnstructured,

	"dwarfToYamlFile": func(data *dwarfy.Data, filename string) error {
		buf, err := data.ToYamlBuffer()
		if err != nil {
			return fmt.Errorf("could not serialize to buffer: %v", err)
		}
		err = ioutil.WriteFile(filename, buf, 0644)
		if err != nil {
			return fmt.Errorf("could not write to file %s: %v", filename, err)
		}
		return nil
	},
	"dwarfToJsonFile": func(data *dwarfy.Data, filename string) error {
		buf, err := data.ToJsonBuffer()
		if err != nil {
			return fmt.Errorf("could not serialize to buffer: %v", err)
		}
		err = ioutil.WriteFile(filename, buf, 0644)
		if err != nil {
			return fmt.Errorf("could not write to file %s: %v", filename, err)
		}
		return nil
	},

	"dwarfToYaml": func(data *dwarfy.Data) (string, error) {
		buf, err := data.ToYamlBuffer()
		if err != nil {
			return "", err
		}
		return string(buf), nil
	},
	"dwarfToJson": func(data *dwarfy.Data) (string, error) {
		buf, err := data.ToJsonBuffer()
		if err != nil {
			return "", err
		}
		return string(buf), nil
	},
	"dwarfToUnstructured": func(data *dwarfy.Data) interface{} {
		return data.ToUnstructured()
	},
	// metadb
	"dwarfToMetaDb": metadb.NewMetaDb,
}

func AppendTemplateFuncMap(funcmap *template.FuncMap) {
	if funcmap == nil {
		return
	}
	for name, fn := range genericMap {
		if fn == nil {
			continue
		}
		_, ok := (*funcmap)[name]
		if ok {
			log.Printf("funcmap already has %s, skipping", name)
		}
		(*funcmap)[name] = fn
	}
}
