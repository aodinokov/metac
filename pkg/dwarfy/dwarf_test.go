package dwarfy

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"debug/elf"
)

// test data files
var elfFiles []string = []string{
	"testdata/c01.elf",
	"testdata/cpp01.elf",
}

func TestJsonInOut(t *testing.T) {
	for _, filename := range elfFiles {
		elfFile, err := elf.Open(filename)
		if err != nil {
			t.Errorf("could not open %s: %v", filename, err)
			continue
		}
		defer elfFile.Close()
		dwarfData, err := elfFile.DWARF()
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}

		dataIn, err := NewData(dwarfData)
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}
		buf, err := dataIn.ToJsonBuffer()
		if err != nil {
			t.Errorf("could not serialize to json %s: %v", filename, err)
			continue
		}
		dataOut, err := NewDataFromJsonBuffer(buf)
		if err != nil {
			t.Errorf("could not deserialize from json %s: %v", filename, err)
			continue
		}

		assert.Equal(t, dataIn, dataOut)

	}
}

func TestYamlInOut(t *testing.T) {
	for _, filename := range elfFiles {
		elfFile, err := elf.Open(filename)
		if err != nil {
			t.Errorf("could not open %s: %v", filename, err)
			continue
		}
		defer elfFile.Close()
		dwarfData, err := elfFile.DWARF()
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}

		dataIn, err := NewData(dwarfData)
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}
		buf, err := dataIn.ToYamlBuffer()
		if err != nil {
			t.Errorf("could not serialize to json %s: %v", filename, err)
			continue
		}
		dataOut, err := NewDataFromYamlBuffer(buf)
		if err != nil {
			t.Errorf("could not deserialize from json %s: %v", filename, err)
			continue
		}

		assert.Equal(t, dataIn, dataOut)
	}
}

func TestUnstructuredInOut(t *testing.T) {
	for _, filename := range elfFiles {
		elfFile, err := elf.Open(filename)
		if err != nil {
			t.Errorf("could not open %s: %v", filename, err)
			continue
		}
		defer elfFile.Close()
		dwarfData, err := elfFile.DWARF()
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}

		dataIn, err := NewData(dwarfData)
		if err != nil {
			t.Errorf("could not get DWARF from %s: %v", filename, err)
			continue
		}
		u := dataIn.ToUnstructured()
		dataOut, err := NewDataFromUnstructured(u)
		if err != nil {
			t.Errorf("could not deserialize from Unstructured %s: %v", filename, err)
			continue
		}

		assert.Equal(t, dataIn, dataOut)
	}
}
