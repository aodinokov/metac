package metadb

import (
	"fmt"

	"github.com/aodinokov/metac/pkg/dwarfy"
)

type CommonId string // uniq id of the entry

// A ICommon conventionally represents a pointer to any of the
// specific Major Entries.
type ICommon interface {
	GetId() CommonId // Id is get assigned when we add to the database
	GetKind() string // kind of object (name of the go Type)
	IsDeclaration() bool
	// signature in the best case scenario is TAG and name
	// but if name is absent we go till the we meet some entry with the name
	// e.g. ARRAY OF(unsigned int) count 5
	Signature(comparable bool) (string, error)
	GetCommonType() *CommonType // will return CommonType mixin if it's Type. nil otherwise
}

type CommonLink struct {
	Kind string
	Id   CommonId
	p    ICommon
}

func (c *CommonLink) Set(in ICommon) {
	c.p = in
	if in != nil {
		_, ok := c.p.(*IndexEntry)
		if !ok {
			c.Kind = in.GetKind()
			c.Id = in.GetId()
		}
	} else {
		c.Id = CommonId("void")
	}
}

func (c *CommonLink) streamline() {
	if c.p == nil {
		return
	}
	index, ok := c.p.(*IndexEntry)
	if !ok {
		c.Kind = c.p.GetKind()
		c.Id = c.p.GetId()
		return
	}
	c.Set(index.p)
}

type Common struct {
	id               CommonId // assigned id (use GetId to get it)
	MixinName        `yaml:",inline"`
	MixinDeclaration `yaml:",inline"`
}

func (c *Common) GetId() CommonId {
	return c.id
}

func (c *Common) fromEntry(entry *dwarfy.Entry) error {
	c.MixinName.fromEntry(entry)
	c.MixinDeclaration.fromEntry(entry)
	return nil
}

func (c *Common) IsDeclaration() bool {
	return c.MixinDeclaration.isDeclaration() && c.MixinName.Name != nil
}

func (c *Common) GetCommonType() *CommonType {
	return nil
}

// Objects (variables and subroutines)
type CommonObj struct {
	Parent      CommonLink
	Common      `yaml:",inline"`
	MixinObject `yaml:",inline"`
}

func (c *CommonObj) fromEntry(entry *dwarfy.Entry) error {
	c.Common.fromEntry(entry)
	c.MixinObject.fromEntry(entry)
	return nil
}

func (s *CommonObj) streamline() {
	s.Parent.streamline()
}

type HierarchyItem struct {
	CommonLink        `yaml:",inline"`
	MixinDeclaration  `yaml:",inline"`
	MixinDeclLocation `yaml:",inline"`
}

type Variable struct {
	CommonObj `yaml:",inline"`

	Type CommonLink
}

func (c *Variable) fromEntry(entry *dwarfy.Entry) error {
	c.CommonObj.fromEntry(entry)
	return nil
}

func (*Variable) GetKind() string {
	return "Variable"
}

func (s *Variable) streamline() {
	s.CommonObj.streamline()
	s.Type.streamline()
}

func (s *Variable) Signature(comparable bool) (string, error) {
	signature := "Variable,"
	if s.Name != nil {
		signature += *s.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if s.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	if s.Type.p != nil {
		x, err := s.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += "Type:(" + x + "),"
	}
	return signature, nil
}

type LexBlock struct {
	Parent CommonLink
	Common `yaml:",inline"`

	Hierarchy []*HierarchyItem `json:",omitempty" yaml:",omitempty"`
}

func (*LexBlock) GetKind() string {
	return "LexBlock"
}

func (s *LexBlock) streamline() {
	s.Parent.streamline()
	for _, x := range s.Hierarchy {
		x.streamline()
	}
}

func (cu *LexBlock) Signature(_ bool) (string, error) {
	return "", fmt.Errorf("signature operation isn't supported for LexDwarfBlock")
}

type Subprogram struct {
	CommonObj `yaml:",inline"`
	MixinFunc `yaml:",inline"`

	Hierarchy []*HierarchyItem `json:",omitempty" yaml:",omitempty"`
}

func (*Subprogram) GetKind() string {
	return "Subprogram"
}

func (s *Subprogram) Signature(comparable bool) (string, error) {
	signature := "Subprogram,"
	if s.Name != nil {
		signature += *s.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if s.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}

	if len(s.Param) > 0 {
		signature += "Takes:("
		for _, p := range s.Param {
			signature += "("
			if p.UnspecifiedParam != nil && *p.UnspecifiedParam {
				signature += "UnspecifiedParam"
			} else {
				if p.Type.p != nil {
					x, err := p.Type.p.Signature(comparable)
					if err != nil {
						return "", err
					}
					signature += x
				}
			}
			signature += "),"
		}
		signature += "),"
	}

	if s.Type.p != nil {
		x, err := s.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += "Returns:(" + x + "),"
	}
	return signature, nil
}

func (s *Subprogram) streamline() {
	s.CommonObj.streamline()
	s.MixinFunc.streamline()
	for _, x := range s.Hierarchy {
		x.streamline()
	}
}

type Namespace struct {
	Parent CommonLink
	Common `yaml:",inline"`

	ImportedModules      []*CommonLink `json:",omitempty" yaml:",omitempty"`
	ImportedDeclarations []*CommonLink `json:",omitempty" yaml:",omitempty"`

	Hierarchy []*HierarchyItem `json:",omitempty" yaml:",omitempty"`
}

func (*Namespace) GetKind() string {
	return "Namespace"
}

func (s *Namespace) Signature(comparable bool) (string, error) {
	signature := "Namespace,"
	if s.Name != nil {
		signature += *s.Name
	}
	return signature, nil
}

func (s *Namespace) streamline() {
	s.Parent.streamline()
	for _, x := range s.ImportedModules {
		x.streamline()
	}
	for _, x := range s.ImportedDeclarations {
		x.streamline()
	}
	for _, x := range s.Hierarchy {
		x.streamline()
	}
}

type CompileUnit struct {
	id        CommonId // assigned id
	DeclFiles []string /* files referenced in DeclFile Attributes*/

	Name       string `json:"name,omitempty" yaml:"name,omitempty"`
	Lang       *dwarfy.Language
	LangFamily *string

	// imported Namespaces
	ImportedModules      []*CommonLink `json:",omitempty" yaml:",omitempty"`
	ImportedDeclarations []*CommonLink `json:",omitempty" yaml:",omitempty"`

	Hierarchy []*HierarchyItem
}

func (*CompileUnit) GetKind() string {
	return "CompileUnit"
}

func (s *CompileUnit) streamline() {
	for _, x := range s.ImportedModules {
		x.streamline()
	}
	for _, x := range s.ImportedDeclarations {
		x.streamline()
	}
	for _, x := range s.Hierarchy {
		x.streamline()
	}
}

func (cu *CompileUnit) GetId() CommonId {
	return cu.id
}
func (cu *CompileUnit) IsDeclaration() bool {
	return false
}
func (cu *CompileUnit) Signature(_ bool) (string, error) {
	return "CompileUnit, Name:" + cu.Name + ",", nil
}
func (cu *CompileUnit) GetCommonType() *CommonType {
	return nil
}

type MetaDb struct {
	// deduplicated types
	BaseTypes        map[CommonId]*BaseType
	EnumerationTypes map[CommonId]*EnumerationType
	ArrayTypes       map[CommonId]*ArrayType
	PointerTypes     map[CommonId]*PointerType
	ReferenceTypes   map[CommonId]*ReferenceType   /* c++ reference and rvalue reference */
	UnspecifiedTypes map[CommonId]*UnspecifiedType /* c++ */
	SubroutineTypes  map[CommonId]*SubroutineType
	QualTypes        map[CommonId]*QualType /*  "const", "restrict", or "volatile" */
	Typedefs         map[CommonId]*Typedef
	ClassTypes       map[CommonId]*StructType
	StructTypes      map[CommonId]*StructType
	UnionTypes       map[CommonId]*StructType

	Subprograms  map[CommonId]*Subprogram
	Variables    map[CommonId]*Variable
	LexBlocks    map[CommonId]*LexBlock
	Namespaces   map[CommonId]*Namespace /* c++ */
	CompileUnits map[CommonId]*CompileUnit
}

func (s *MetaDb) streamline() {
	for _, x := range s.BaseTypes {
		x.streamline()
	}
	for _, x := range s.EnumerationTypes {
		x.streamline()
	}
	for _, x := range s.ArrayTypes {
		x.streamline()
	}
	for _, x := range s.PointerTypes {
		x.streamline()
	}
	for _, x := range s.ReferenceTypes {
		x.streamline()
	}
	for _, x := range s.UnspecifiedTypes {
		x.streamline()
	}
	for _, x := range s.SubroutineTypes {
		x.streamline()
	}
	for _, x := range s.QualTypes {
		x.streamline()
	}
	for _, x := range s.Typedefs {
		x.streamline()
	}
	for _, x := range s.ClassTypes {
		x.streamline()
	}
	for _, x := range s.StructTypes {
		x.streamline()
	}
	for _, x := range s.UnionTypes {
		x.streamline()
	}
	for _, x := range s.Subprograms {
		x.streamline()
	}
	for _, x := range s.Variables {
		x.streamline()
	}
	for _, x := range s.LexBlocks {
		x.streamline()
	}
	for _, x := range s.Namespaces {
		x.streamline()
	}
	for _, x := range s.CompileUnits {
		x.streamline()
	}
}
