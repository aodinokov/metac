package metadb

import (
	"fmt"

	"github.com/aodinokov/metac/pkg/dwarfy"
)

type MixinName struct {
	Name *string `json:",omitempty" yaml:",omitempty"`
}

func (m *MixinName) fromEntry(entry *dwarfy.Entry) error {
	name, ok := entry.Val("Name").(string)
	if ok {
		m.Name = &name
	}
	return nil
}

type MixinDeclaration struct {
	Declaration *bool `json:",omitempty" yaml:",omitempty"`
}

func (m *MixinDeclaration) fromEntry(entry *dwarfy.Entry) error {
	declaration, ok := entry.Val("Declaration").(bool)
	if ok {
		m.Declaration = &declaration
	}
	return nil
}

func (m *MixinDeclaration) isDeclaration() bool {
	return m.Declaration != nil && *m.Declaration
}

type MixinByteSize struct {
	ByteSize *int64 `json:",omitempty" yaml:",omitempty"`
	Alignment *int64 `json:",omitempty" yaml:",omitempty"`
}

func (m *MixinByteSize) fromEntry(entry *dwarfy.Entry) error {
	byteSize, ok := entry.Val("ByteSize").(int64)
	if ok {
		m.ByteSize = &byteSize
	}
	// alignment goes together with size
	alignment, ok := entry.Val("Alignment").(int64)
	if ok {
		m.Alignment = &alignment
	}
	return nil
}


type MixinObject struct {
	External    *bool   `json:",omitempty" yaml:",omitempty"`
	LinkageName *string `json:",omitempty" yaml:",omitempty"`
}

func (m *MixinObject) fromEntry(entry *dwarfy.Entry) error {
	external, ok := entry.Val("External").(bool)
	if ok {
		m.External = &external
	}
	name, ok := entry.Val("LinkageName").(string)
	if ok {
		m.LinkageName = &name
	}
	return nil
}

type MixinDeclLocation struct {
	DeclFile   *int64 `json:",omitempty" yaml:",omitempty"`
	DeclLine   *int64 `json:",omitempty" yaml:",omitempty"`
	DeclColumn *int64 `json:",omitempty" yaml:",omitempty"`
}

func (m *MixinDeclLocation) fromEntry(entry *dwarfy.Entry) error {
	declFile, ok := entry.Val("DeclFile").(int64)
	if ok {
		m.DeclFile = &declFile
	}
	declLine, ok := entry.Val("DeclLine").(int64)
	if ok {
		m.DeclLine = &declLine
	}
	declColumn, ok := entry.Val("DeclColumn").(int64)
	if ok {
		m.DeclColumn = &declColumn
	}
	return nil
}

func (m *MixinDeclLocation) toString(cu *CompileUnit) string {
	if cu == nil {
		return "<invalid>"
	}
	res := ""
	if m.DeclFile != nil {
		res += cu.DeclFiles[int(*m.DeclFile)]
		if m.DeclLine != nil {
			res += fmt.Sprintf(":%d", *m.DeclLine)
			if m.DeclColumn != nil {
				res += fmt.Sprintf(":%d", *m.DeclColumn)
			}
		}
	}
	return res
}

type FuncParam struct {
	UnspecifiedParam *bool `json:"unspecifiedparam,omitempty" yaml:"unspecifiedparam,omitempty"` // if param is unspecified
	// otherwise
	Type      CommonLink
	MixinName `json:",omitempty" yaml:",omitempty"`
}

func (m *FuncParam) fromEntry(entry *dwarfy.Entry) error {
	m.MixinName.fromEntry(entry)
	return nil
}

func (s *FuncParam) streamline() {
	s.Type.streamline()
}

// A MixinFunc represents a function type.
type MixinFunc struct {
	Type  CommonLink   // return type
	Param []*FuncParam `json:"param,omitempty" yaml:"param,omitempty"`
}

func (s *MixinFunc) streamline() {
	s.Type.streamline()
	for _, f := range s.Param {
		f.streamline()
	}
}
