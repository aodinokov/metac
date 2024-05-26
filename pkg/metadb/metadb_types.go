package metadb

import (
	"fmt"

	"github.com/aodinokov/metac/pkg/dwarfy"
)

// Types:
// A CommonType holds fields common to multiple types.
// If a field is not known or not applicable for a given type,
// the zero value is used.
type CommonType struct {
	Parents       []*CommonLink
	Common        `yaml:",inline"`
	MixinByteSize `yaml:",inline"`
}

func (c *CommonType) fromEntry(entry *dwarfy.Entry) error {
	c.Common.fromEntry(entry)
	c.MixinByteSize.fromEntry(entry)

	return nil
}

func (s *CommonType) streamline() {
	for _, parent := range s.Parents {
		parent.streamline()
	}
}

func (s *CommonType) GetCommonType() *CommonType {
	return s
}

var baseTypeSynonims = map[string]string{
	"short":            "short int",
	"signed short":     "short int",
	"short signed":     "short int",
	"signed short int": "short int",
	"short signed int": "short int",

	"unsigned short":     "unsigned short int",
	"short unsigned":     "unsigned short int",
	"unsigned short int": "unsigned short int",
	"short unsigned int": "unsigned short int",

	"signed int": "int",
	"signed":     "int",

	"unsigned": "unsigned int",

	"long":            "long int",
	"signed long":     "long int",
	"long signed ":    "long int",
	"signed long int": "long int",
	"long signed int": "long int",

	"unsigned long":     "unsigned long int",
	"long unsigned ":    "unsigned long int",
	"unsigned long int": "unsigned long int",
	"long unsigned int": "unsigned long int",

	"long long":            "long long int",
	"signed long long":     "long long int",
	"signed long long int": "long long int",

	"unsigned long long":     "unsigned long long int",
	"long long unsigned":     "unsigned long long int",
	"long long unsigned int": "unsigned long long int",

	/* clang has only complex. with different size */
	"complex float":       "complex",
	"float complex":       "complex",
	"complex double":      "complex",
	"double complex":      "complex",
	"long double complex": "complex",
	"complex long double": "complex",
	"long complex double": "complex",
}

func deSynonim(in string) string {
	out, ok := baseTypeSynonims[in]
	if ok {
		return out
	}
	return in
}

// A BaseType holds fields common to all basic types.
type BaseType struct {
	CommonType `yaml:",inline"`
	Encoding   string // signed, unsiged etc
}

func (*BaseType) GetKind() string {
	return "BaseType"
}

func (baseType *BaseType) fromEntry(entry *dwarfy.Entry) error {
	baseType.CommonType.fromEntry(entry)
	if baseType.Name == nil {
		return fmt.Errorf("the BaseType must have Name")
	}
	// correct name of type to avoid synonims. e.g. if long use long int (or vice-versa)
	dsname := deSynonim(*baseType.Name)
	baseType.Name = &dsname

	encoding, ok := entry.Val("Encoding").(int64)
	if ok {
		baseType.Encoding = dwarfy.Encoding(encoding).String()
	}

	// highlight the incompatiblity with what compiler generated
	_, ok = entry.Val("BitSize").(int64)
	if ok {
		return fmt.Errorf("current implementation doesn't expect BitSize attr for base type")
	}
	_, ok = entry.Val("BitOffset").(int64)
	if ok {
		return fmt.Errorf("current implementation doesn't expect BitOffset attr for base type")
	}
	_, ok = entry.Val("DataBitOffset").(int64)
	if ok {
		return fmt.Errorf("current implementation doesn't expect DataBitOffset attr for base type")
	}

	return nil
}

func (b *BaseType) Signature(comparable bool) (string, error) {
	if b.Name == nil {
		return "", fmt.Errorf("the BaseType must have Name")
	}
	signature := "BaseType,"
	signature += *b.Name
	if !comparable {
		return signature, nil
	}
	comparable = false
	if b.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	signature += "Encoding:" + b.Encoding + ","
	if b.ByteSize != nil {
		signature += "ByteSize:" + fmt.Sprintf("%d", *b.ByteSize) + ","
	}
	return signature, nil
}

type EnumerationType struct {
	CommonType  `yaml:",inline"`
	Enumerators []struct {
		Key string
		Val int64
	}
}

func (*EnumerationType) GetKind() string {
	return "EnumerationType"
}

func (e *EnumerationType) Signature(comparable bool) (string, error) {
	signature := "EnumerationType,"
	if e.Name != nil {
		signature += *e.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if e.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	signature += "Enumberators:("
	for _, v := range e.Enumerators {
		signature += "(" + v.Key + ":" + fmt.Sprintf("%d", v.Val) + "),"
	}
	signature += "),"
	return signature, nil
}

// A QualType represents a type that has the C/C++ "const", "restrict", or "volatile" qualifier.
type QualType struct {
	CommonType `yaml:",inline"`
	Qual       string

	Type CommonLink
}

func (*QualType) GetKind() string {
	return "QualType"
}

func (s *QualType) streamline() {
	s.CommonType.streamline()
	s.Type.streamline()
}

func (q *QualType) Signature(comparable bool) (string, error) {
	signature := "QualType:" + q.Qual + ","
	if q.Name != nil {
		signature += *q.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if q.Type.p != nil {
		signature += "From:("
		s, err := q.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += s + "),"
	}
	return signature, nil
}

// A Typedef represents a named type.
type Typedef struct {
	CommonType `yaml:",inline"`

	Type CommonLink
}

func (s *Typedef) streamline() {
	s.CommonType.streamline()
	s.Type.streamline()
}

func (*Typedef) GetKind() string {
	return "Typedef"
}

func (t *Typedef) Signature(comparable bool) (string, error) {
	if t.Name == nil {
		return "", fmt.Errorf("the Typedef must have Name")
	}
	signature := "Typedef,"
	if t.Name != nil {
		signature += *t.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if t.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	if t.Type.p != nil {
		signature += "From:("
		s, err := t.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += s + "),"
	}
	return signature, nil
}

// A PointerType represents a pointer type.
type PointerType struct {
	CommonType `yaml:",inline"`

	Type CommonLink
}

func (*PointerType) GetKind() string {
	return "PointerType"
}

func (s *PointerType) streamline() {
	s.CommonType.streamline()
	s.Type.streamline()
}

func (p *PointerType) Signature(comparable bool) (string, error) {
	signature := "PointerType,"
	if p.Name != nil {
		signature += *p.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if p.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	if p.Type.p != nil {
		signature += "From:("
		s, err := p.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += s + "),"
	}
	return signature, nil
}

// A ReferenceType represents a pointer type.
type UnspecifiedType struct {
	CommonType `yaml:",inline"`
}

func (*UnspecifiedType) GetKind() string {
	return "UnspecifiedType"
}

func (p *UnspecifiedType) Signature(comparable bool) (string, error) {
	signature := "UnspecifiedType,"
	if p.Name != nil {
		signature += *p.Name
		if !comparable {
			return signature, nil
		}
	}
	return signature, nil
}

// A ReferenceType represents a pointer type.
type ReferenceType struct {
	CommonType `yaml:",inline"`

	Tag string

	Type CommonLink
}

func (*ReferenceType) GetKind() string {
	return "ReferenceType"
}

func (s *ReferenceType) streamline() {
	s.CommonType.streamline()
	s.Type.streamline()
}

func (p *ReferenceType) Signature(comparable bool) (string, error) {
	signature := p.Tag + ","
	if p.Name != nil {
		signature += *p.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if p.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}
	if p.Type.p != nil {
		signature += "From:("
		s, err := p.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += s + "),"
	}
	return signature, nil
}

// typically used for Func ptr
type SubroutineType struct {
	CommonType `yaml:",inline"`
	MixinFunc  `yaml:",inline"`
}

func (*SubroutineType) GetKind() string {
	return "SubroutineType"
}

func (s *SubroutineType) streamline() {
	s.CommonType.streamline()
	s.MixinFunc.streamline()
}

func (s *SubroutineType) Signature(comparable bool) (string, error) {
	signature := "SubroutineType,"
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

// An ArrayType represents a fixed size array type.
type ArrayType struct {
	CommonType `yaml:",inline"`

	Type CommonLink

	StrideBitSize *int64 `json:"stridebitsize,omitempty" yaml:"stridebitsize,omitempty"` // number of bits to hold each element
	LowerBound    *int64 `json:"lowerbound,omitempty" yaml:"lowerbound,omitempty"`
	Count         int64  // -1 if array is flexible. (in C it can be only the last subrange (highest level) in case of mutlidimentional)
	Level         int64  // keeps Subrange id
}

func (*ArrayType) GetKind() string {
	return "ArrayType"
}

func (s *ArrayType) streamline() {
	s.CommonType.streamline()
	s.Type.streamline()
}

func (a *ArrayType) Signature(comparable bool) (string, error) {
	signature := "ArrayType,"
	if a.Name != nil {
		signature += *a.Name
		if !comparable {
			return signature, nil
		}
	}
	comparable = false
	if a.isDeclaration() {
		return "", fmt.Errorf("can't create comparable signature from declaration")
	}

	if a.StrideBitSize != nil {
		signature += "StrideBitSize:" + fmt.Sprintf("%d", *a.StrideBitSize) + ","
	}
	if a.LowerBound != nil {
		signature += "LowerBound:" + fmt.Sprintf("%d", *a.LowerBound) + ","
	}
	signature += "Count:" + fmt.Sprintf("%d", a.Count) + ","
	signature += "Level:" + fmt.Sprintf("%d", a.Level) + ","

	if a.Type.p != nil {
		signature += "From:("
		x, err := a.Type.p.Signature(comparable)
		if err != nil {
			return "", err
		}
		signature += x + "),"
	}

	return signature, nil
}

// A StructField represents a field in a struct, union, or C++ class type.
type StructField struct {
	Name *string `json:"name,omitempty" yaml:"name,omitempty"` // there can be anonymous fields in structs if they're branches

	Type CommonLink

	ByteSize *int64 `json:"bytesize,omitempty" yaml:"bytesize,omitempty"` // will be if defined

	ByteOffset    int64  `json:"byteoffset,omitempty" yaml:"byteoffset,omitempty"`       // mutually exclusive with the next
	DataBitOffset *int64 `json:"databitoffset,omitempty" yaml:"databitoffset,omitempty"` // nil if not a bit field

	BitOffset *int64 `json:"bitoffset,omitempty" yaml:"bitoffset,omitempty"` // nil if not a bit field
	BitSize   *int64 `json:"bitsize,omitempty" yaml:"bitsizes,omitempty"`    // nil if not a bit field
}

func (s *StructField) streamline() {
	s.Type.streamline()
}

// A StructType represents a struct, union, or C++ class type.
type StructType struct {
	CommonType `yaml:",inline"`
	Tag        string
	Fields     []*StructField `yaml:",omitempty"`

	Hierarchy []*HierarchyItem `json:",omitempty" yaml:",omitempty"`
}

func (s *StructType) GetKind() string {
	return s.Tag
}

func (s *StructType) streamline() {
	s.CommonType.streamline()
	for _, f := range s.Fields {
		f.streamline()
	}
	for _, x := range s.Hierarchy {
		x.streamline()
	}
}

func (s *StructType) Signature(comparable bool) (string, error) {
	signature := "Tag:" + s.Tag + ","
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
	if len(s.Fields) > 0 {
		signature += "Fields:("
		for _, p := range s.Fields {
			signature += "("
			if p.Name != nil {
				signature += "Name:" + *p.Name + ","
			}
			if p.Type.p != nil {
				signature += "Type:("
				x, err := p.Type.p.Signature(comparable)
				if err != nil {
					return "", err
				}
				signature += x + "),"
			}
			if p.BitSize != nil {
				signature += "BitSize:" + fmt.Sprintf("%d", *p.BitSize) + ","
			}
			if p.BitOffset != nil {
				signature += "BitOffset:" + fmt.Sprintf("%d", *p.BitOffset) + ","
			} else {
				signature += "ByteOffset:" + fmt.Sprintf("%d", p.ByteOffset) + ","
			}
			if p.DataBitOffset != nil {
				signature += "DataBitOffset:" + fmt.Sprintf("%d", *p.DataBitOffset) + ","
			}

			signature += "),"
		}
		signature += "),"
	}
	if len(s.Fields) > 0 {
		signature += "Other:("
		for _, h := range s.Hierarchy {
			signature += "("
			if h.p == nil {
				return "", fmt.Errorf("hierachy element of struct contains nil pointer")
			}
			x, err := h.p.Signature(comparable)
			if err != nil {
				return "", err
			}
			signature += x
			signature += "),"
		}
		signature += "),"
	}

	return signature, nil
}

func isTypeTag(tag dwarfy.Tag) bool {
	switch tag {
	case "BaseType":
		fallthrough
	case "EnumerationType":
		fallthrough
	case "ArrayType":
		fallthrough
	case "PointerType":
		fallthrough
	case "ReferenceType":
		fallthrough
	case "RvalueReferenceType":
		fallthrough
	case "UnspecifiedType":
		fallthrough
	case "SubroutineType":
		fallthrough
	case "Typedef":
		fallthrough
	case "ConstType":
		fallthrough
	case "VolatileType":
		fallthrough
	case "RestrictType":
		fallthrough
	case "MutableType":
		fallthrough
	case "SharedType":
		fallthrough
	case "PackedType":
		fallthrough
	case "ClassType":
		fallthrough
	case "StructType":
		fallthrough
	case "UnionType":
		return true
	}
	return false
}
