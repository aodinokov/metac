package dwarfy

import (
	"debug/dwarf"
	"encoding/json"
	"fmt"
	"strconv"

	"gopkg.in/yaml.v3"
)

/*
	dwarfy (from dwarf-yaml)
	the intent is to make yaml/json serializable-deserializable format of dwarf
	it also has ability to be converted to/from unstructured to be able to work
	natively with gotemplate sprig dict,list and etc
*/

// use the same type to avoid convertation issues:
// An Offset represents the location of an Entry within the DWARF info.
type Offset dwarf.Offset

func NewOffsetFromUnstructured(u interface{}) (Offset, error) {
	cU, ok := u.(string)
	if !ok {
		return 0, fmt.Errorf("couldn't cast Parent %v to string", u)
	}
	intU, err := strconv.Atoi(cU)
	if err != nil {
		return 0, fmt.Errorf("couldn't cast Parent %s to Offset", cU)
	}
	return Offset(intU), nil

}

func (o Offset) ToUnstructured() interface{} {
	return fmt.Sprintf("%d", o)
}

// A Tag is the classification (the type) of an Entry.
type Tag string

// A Class is the DWARF 4 class of an attribute value.
type Class string

// An Attr identifies the attribute type in a DWARF Entry's Field.
type Attr string

// A Field is a single attribute/value pair in an Entry.
type Field struct {
	Attr  Attr        `json:"attr,omitempty" yaml:"attr,omitempty"`
	Val   interface{} `json:"val,omitempty" yaml:"val,omitempty"`
	Class Class       `json:"class,omitempty" yaml:"class,omitempty"`
}

func NewField(dwarfField *dwarf.Field) (*Field, error) {
	field := Field{
		Attr:  Attr(dwarfField.Attr.String()),
		Val:   dwarfField.Val,
		Class: Class(dwarfField.Class.String()),
	}

	field.fixValBasedOnClass()

	return &field, nil
}

func NewFieldFromUnstructured(iu interface{}) (*Field, error) {
	var m *map[string]interface{}

	switch u := iu.(type) {
	case *map[string]interface{}:
		m = u
	case map[string]interface{}:
		m = &u
	default:
		return nil, fmt.Errorf("couldn't cast %v to map", u)
	}

	field := Field{}

	// Attr
	iAttr, ok := (*m)["Attr"]
	if !ok {
		return nil, fmt.Errorf("couldn't find mandatory field Attr in %v", *m)
	}
	cAttr, ok := iAttr.(string)
	if !ok {
		return nil, fmt.Errorf("couldn't cast %v to string", cAttr)
	}
	field.Attr = Attr(cAttr)
	// Class
	iClass, ok := (*m)["Class"]
	if !ok {
		return nil, fmt.Errorf("couldn't find mandatory field Class in %v", *m)
	}
	cClass, ok := iClass.(string)
	if !ok {
		return nil, fmt.Errorf("couldn't cast %v to string", cClass)
	}
	field.Class = Class(cClass)
	// Val
	iVal, ok := (*m)["Val"]
	if !ok {
		return nil, fmt.Errorf("couldn't find mandatory field Val in %v", *m)
	}
	if field.Class == "ClassReference" {
		offset, err := NewOffsetFromUnstructured(iVal)
		if err != nil {
			return nil, fmt.Errorf("couldn't convert %v to offset", iVal)
		}
		field.Val = offset
	} else {
		field.Val = iVal
	}

	field.fixValBasedOnClass()
	return &field, nil
}

func (f *Field) ToUnstructured() interface{} {
	val := f.Val
	offset, ok := val.(Offset)
	if f.Class == "ClassReference" && ok {
		// need to convert from Offset to something std
		val = fmt.Sprintf("%d", offset)
	}
	return map[string]interface{}{
		"Attr":  string(f.Attr),
		"Val":   val,
		"Class": string(f.Class),
	}
}

func (f *Field) fixValBasedOnClass() {
	// A value can be one of several "attribute classes" defined by DWARF.
	// The Go types corresponding to each class are:
	//
	//    DWARF class       Go type        Class
	//    -----------       -------        -----
	//    address           uint64         ClassAddress
	//    block             []byte         ClassBlock -> we convert to []int so json didn't conver it to string
	//    constant          int64          ClassConstant
	//    flag              bool           ClassFlag
	//    reference
	//      to info         dwarf.Offset   ClassReference
	//      to type unit    uint64         ClassReferenceSig
	//    string            string         ClassString
	//    exprloc           []byte         ClassExprLoc        -> we convert to []int so json didn't conver it to string
	//    lineptr           int64          ClassLinePtr
	//    loclistptr        int64          ClassLocListPtr
	//    macptr            int64          ClassMacPtr
	//    rangelistptr      int64          ClassRangeListPtr
	//
	// For unrecognized or vendor-defined attributes, Class may be
	// ClassUnknown.

	switch f.Class {
	case "ClassAddress":
		fallthrough
	case "ClassReferenceSig":
		switch f.Val.(type) {
		case float64:
			f.Val = uint64(f.Val.(float64))
		case int:
			f.Val = uint64(f.Val.(int))
		}
	case "ClassReference":
		switch f.Val.(type) {
		case float64:
			f.Val = Offset(f.Val.(float64))
		case int:
			f.Val = Offset(f.Val.(int))
		case dwarf.Offset:
			f.Val = Offset(f.Val.(dwarf.Offset))
		}
	case "ClassConstant":
		fallthrough
	case "ClassLinePtr":
		fallthrough
	case "ClassLocListPtr":
		fallthrough
	case "ClassMacPtr":
		fallthrough
	case "ClassRangeListPtr":
		fallthrough
	case "ClassUnknown":
		switch f.Val.(type) {
		case float64:
			f.Val = int64(f.Val.(float64))
		case int:
			f.Val = int64(f.Val.(int))
		}
	case "ClassExprLoc":
		fallthrough
	case "ClassBlock":
		switch f.Val.(type) {
		case []byte:
			bVals := f.Val.([]byte)
			var r []int
			for _, bVal := range bVals {
				r = append(r, int(bVal))
			}
			f.Val = r
		case []interface{}: // produced by yaml and json module
			iVals := f.Val.([]interface{})
			var r []int
			for _, iVal := range iVals {
				switch val := iVal.(type) {
				case int:
					r = append(r, int(val))
				case float64:
					r = append(r, int(val))
				}
			}
			f.Val = r
		}
	}
}

// An entry is a sequence of attribute/value pairs.
type Entry struct {
	Parent   *Offset  `json:"parent,omitempty" yaml:"parent,omitempty"`     // if the entry has parent show it
	Tag      Tag      `json:"kind,omitempty" yaml:"kind,omitempty"`         // tag (kind of Entry)
	Fields   []*Field `json:"fields,omitempty" yaml:"fields,omitempty"`     // attributes
	Children []Offset `json:"children,omitempty" yaml:"children,omitempty"` // children

	// kind-specific extra fields
	DeclFiles *[]string `json:"declFiles,omitempty" yaml:"declFiles,omitempty"` /* files referenced in DeclFile Attributes - cu specific only */
}

func NewEntry(dwarfData *dwarf.Data, dwarfEntry *dwarf.Entry) (*Entry, error) {
	entry := &Entry{Tag: Tag(dwarfEntry.Tag.String())}

	// copy fields
	for _, dwarfField := range dwarfEntry.Field {
		field, err := NewField(&dwarfField)
		if err != nil {
			return nil, err
		}
		entry.Fields = append(entry.Fields, field)
	}

	// in addition: read DeclFiles if it's complie unit
	if dwarfEntry.Tag == dwarf.TagCompileUnit {
		lreader, err := dwarfData.LineReader(dwarfEntry)
		if err != nil {
			return nil, err
		}
		var declFiles []string
		for _, f := range lreader.Files() {
			if f == nil {
				// to keep the same index
				declFiles = append(declFiles, "")
				continue
			}
			declFiles = append(declFiles, f.Name)
		}
		entry.DeclFiles = &declFiles
	}

	return entry, nil
}

func NewEntryFromUnstructured(iu interface{}) (*Entry, error) {
	var m *map[string]interface{}

	switch u := iu.(type) {
	case *map[string]interface{}:
		m = u
	case map[string]interface{}:
		m = &u
	default:
		return nil, fmt.Errorf("couldn't cast %v to map", u)
	}

	entry := Entry{}

	// Parent
	iParent, ok := (*m)["Parent"]
	if ok {
		cParent, ok := iParent.(string)
		if !ok {
			return nil, fmt.Errorf("couldn't cast Parent %v to string", iParent)
		}
		intParent, err := strconv.Atoi(cParent)
		if err != nil {
			return nil, fmt.Errorf("couldn't cast Parent %s to Offset", cParent)
		}
		parent := Offset(intParent)
		entry.Parent = &parent
	}
	// Tag
	iTag, ok := (*m)["Tag"]
	if !ok {
		return nil, fmt.Errorf("couldn't find mandatory Tag field in %v", *m)
	}
	cTag, ok := iTag.(string)
	if !ok {
		return nil, fmt.Errorf("couldn't cast Tag %v to string", iTag)
	}
	entry.Tag = Tag(cTag)
	// Fields
	iFields, ok := (*m)["Fields"]
	if ok {
		cFields, ok := iFields.([]interface{})
		if !ok {
			return nil, fmt.Errorf("couldn't cast Fields %v to array", iFields)
		}
		for _, iField := range cFields {
			field, err := NewFieldFromUnstructured(iField)
			if err != nil {
				return nil, fmt.Errorf("couldn't cast Field %v to array: %v", iField, err)
			}
			entry.Fields = append(entry.Fields, field)
		}
	}
	// Children
	iChildren, ok := (*m)["Children"]
	if ok {
		cChildren, ok := iChildren.([]interface{})
		if !ok {
			return nil, fmt.Errorf("couldn't cast Children %v to array", iChildren)
		}
		for _, iChild := range cChildren {
			child, err := NewOffsetFromUnstructured(iChild)
			if err != nil {
				return nil, fmt.Errorf("couldn't cast Child %v to offset: %v", iChild, err)
			}
			entry.Children = append(entry.Children, child)
		}
	}
	// DeclFiles
	iDeclFiles, ok := (*m)["DeclFiles"]
	if ok {
		cDeclFiles, ok := iDeclFiles.([]interface{})
		if !ok {
			return nil, fmt.Errorf("couldn't cast DeclFiles %v to array", iChildren)
		}
		declFiles := []string{}
		for _, iDeclFile := range cDeclFiles {
			declFile, ok := iDeclFile.(string)
			if !ok {
				return nil, fmt.Errorf("couldn't cast DeclFile %v to string", iDeclFile)
			}
			declFiles = append(declFiles, declFile)
		}
		entry.DeclFiles = &declFiles
	}

	entry.fixValBasedOnClass()
	return &entry, nil
}

func (e *Entry) ToUnstructured() interface{} {
	u := map[string]interface{}{
		"Tag": string(e.Tag),
	}
	if e.Parent != nil {
		u["Parent"] = fmt.Sprintf("%d", uint32(*e.Parent))
	}
	if len(e.Fields) > 0 {
		var iFields []interface{}
		for _, f := range e.Fields {
			iFields = append(iFields, f.ToUnstructured())
		}
		u["Fields"] = iFields
	}
	if len(e.Children) > 0 {
		var iChildren []interface{}
		for _, child := range e.Children {
			iChildren = append(iChildren, child.ToUnstructured())
		}
		u["Children"] = iChildren

	}
	if e.DeclFiles != nil && len(*e.DeclFiles) > 0 {
		var iDeclFiles []interface{}
		for _, declFile := range *e.DeclFiles {
			iDeclFiles = append(iDeclFiles, interface{}(declFile))
		}
		u["DeclFiles"] = iDeclFiles

	}
	return u
}

func (e *Entry) fixValBasedOnClass() {
	for _, f := range e.Fields {
		f.fixValBasedOnClass()
	}
}

// Val returns the value associated with attribute Attr in Entry,
// or nil if there is no such attribute.
//
// A common idiom is to merge the check for nil return with
// the check that the value has the expected dynamic type, as in:
//
//	v, ok := e.Val(AttrSibling).(int64)
func (e *Entry) Val(a Attr) interface{} {
	if f := e.AttrField(a); f != nil {
		return f.Val
	}
	return nil
}

// AttrField returns the Field associated with attribute Attr in
// Entry, or nil if there is no such attribute.
func (e *Entry) AttrField(a Attr) *Field {
	for i, f := range e.Fields {
		if f.Attr == a {
			return e.Fields[i]
		}
	}
	return nil
}

// should be the same as dwarf.Data, but serializable
// this allows to share it with other applications
type Data struct {
	Entries map[Offset]*Entry `json:"entries,omitempty" yaml:"entries,omitempty"`
}

func NewData(dwarfData *dwarf.Data) (*Data, error) {
	data := &Data{Entries: map[Offset]*Entry{}}
	data.Entries[0] = &Entry{}

	var parentsStack []struct {
		Offset  Offset
		Sibling Offset // 0 if not defined
	} = []struct {
		Offset  Offset
		Sibling Offset
	}{{Offset: 0}}

	// go through the DWARF
	reader := dwarfData.Reader()
	for {
		dwarfEntry, err := reader.Next()
		if dwarfEntry == nil && err == nil {
			//log.Printf("done\n")
			break
		}
		if err != nil {
			return nil, err
		}
		// don't need to serialize this since we're doing hierarhy with Children field
		if dwarfEntry.Tag == 0 {
			// clang doesn't use siblings, so we need to lower the level
			if len(parentsStack) > 1 {
				parentsStack = parentsStack[:len(parentsStack)-1]
			}
			continue
		}

		// CompileUnits not always have Siblings set, but we know that they always belong to "0"
		if dwarfEntry.Tag.String() == "CompileUnit" {
			parentsStack = []struct {
				Offset  Offset
				Sibling Offset
			}{{Offset: 0}}
		} else {
			// handle children end using sibling
			//log.Printf("Offset: %d, Stack: %v\n", Offset(dwarfEntry.Offset), parentsStack)
			for i := range parentsStack {
				rev_i := len(parentsStack) - i - 1
				parentsStackEntry := parentsStack[rev_i]
				if parentsStackEntry.Sibling != 0 &&
					parentsStackEntry.Sibling == Offset(dwarfEntry.Offset) {
					// we need to delete the stack till that level
					parentsStack = parentsStack[:rev_i]
					//log.Printf("Updated Stack: %v\n", parentsStack)
					break
				}
			}
		}

		// create Entry
		entry, err := NewEntry(dwarfData, dwarfEntry)
		if err != nil {
			return nil, err
		}
		data.Entries[Offset(dwarfEntry.Offset)] = entry

		// handle parent/child relationship and set the fields correctly
		if len(parentsStack) > 0 {
			parentOffset := parentsStack[len(parentsStack)-1].Offset
			// get parent from our stack's last element
			entry.Parent = &parentOffset
			// put our offset into the parent's child list
			data.Entries[parentOffset].Children = append(data.Entries[*entry.Parent].Children, Offset(dwarfEntry.Offset))
		}
		// the next will be our children - add info to our stack
		if dwarfEntry.Children {
			parentStackEntry := struct {
				Offset  Offset
				Sibling Offset
			}{Offset: Offset(dwarfEntry.Offset)}

			sibling, ok := dwarfEntry.Val(dwarf.AttrSibling).(dwarf.Offset)
			if ok {
				parentStackEntry.Sibling = Offset(sibling)
			}
			parentsStack = append(parentsStack, parentStackEntry)

		}
	}
	return data, nil
}

func NewDataFromUnstructured(iu interface{}) (*Data, error) {
	var m *map[string]interface{}

	switch u := iu.(type) {
	case *map[string]interface{}:
		m = u
	case map[string]interface{}:
		m = &u
	default:
		return nil, fmt.Errorf("couldn't cast %v to map", u)
	}

	data := Data{Entries: map[Offset]*Entry{}}
	// Entries
	iEntries, ok := (*m)["Entries"]
	if !ok {
		return nil, fmt.Errorf("couldn't find Entries in %v", *m)
	}
	cEntries, ok := iEntries.(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("couldn't cast %v to map[string]interface{}", iEntries)
	}
	for k, iV := range cEntries {
		intOffset, err := strconv.Atoi(k)
		if err != nil {
			return nil, fmt.Errorf("couldn't convert %s to int", k)
		}
		data.Entries[Offset(intOffset)], err = NewEntryFromUnstructured(iV)
		if err != nil {
			return nil, fmt.Errorf("couldn't convert entry from %v: %v", iV, err)
		}
	}
	data.fixValBasedOnClass()
	return &data, nil
}

func NewDataFromJsonBuffer(buf []byte) (*Data, error) {
	data := Data{}
	err := json.Unmarshal(buf, &data)
	if err != nil {
		return nil, err
	}
	data.fixValBasedOnClass()
	return &data, nil
}

func NewDataFromYamlBuffer(buf []byte) (*Data, error) {
	data := Data{}
	err := yaml.Unmarshal(buf, &data)
	if err != nil {
		return nil, err
	}
	data.fixValBasedOnClass()
	return &data, nil
}

func (data *Data) ToUnstructured() interface{} {
	uEntries := map[string]interface{}{}
	u := map[string]interface{}{
		"Entries": uEntries,
	}
	for k, entry := range data.Entries {
		uEntries[fmt.Sprintf("%d", k)] = entry.ToUnstructured()
	}
	return u
}

func (data *Data) fixValBasedOnClass() {
	for _, e := range data.Entries {
		e.fixValBasedOnClass()
	}
}

func (data *Data) ToJsonBuffer() ([]byte, error) {
	return json.Marshal(data)
}

func (data *Data) ToYamlBuffer() ([]byte, error) {
	return yaml.Marshal(data)
}
