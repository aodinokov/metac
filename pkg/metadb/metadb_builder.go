package metadb

import (
	"fmt"
	"log"
	"path/filepath"

	"github.com/aodinokov/metac/pkg/dwarfy"
	"github.com/huandu/xstrings"
)

/*
The algorithm is the following:
go via all offsets,
don't try open early references via Type links - just create indexEntry to resolve it when time comes
This approach helps to resolve all circular links
once done work on deduplication - optional
once done - streamline links to get rid of IndexEntry
once done - find all exported objects on global level + objects with linkageName and generate exported symbols table
*/

type IndexEntry struct {
	Offset dwarfy.Offset
	Entry  *dwarfy.Entry

	// some items from Entry
	Tag dwarfy.Tag
	MixinDeclaration
	MixinName // if isDeclaration is true - Name will must always present

	cu *CompileUnit
	MixinDeclLocation

	CommonLink

	isDuplicate bool
}

func (*IndexEntry) GetKind() string {
	return ""
}

func (s *IndexEntry) Signature(comparable bool) (string, error) {
	if s.p == nil {
		panic("IndexEntry pointer is nil")
	}
	return s.p.Signature(comparable)
}

func (s *IndexEntry) GetId() CommonId {
	if s.p == nil {
		return "unresolved"
	}
	return s.p.GetId()
}

func (s *IndexEntry) IsDeclaration() bool {
	if s.p == nil {
		s.isDeclaration()
	}
	return s.p.IsDeclaration()
}

func (s *IndexEntry) GetCommonType() *CommonType {
	if s.p == nil {
		return nil
	}
	return s.p.GetCommonType()
}

func (index *IndexEntry) fromEntry(entry *dwarfy.Entry) error {
	index.Entry = entry
	index.Tag = entry.Tag

	index.MixinDeclLocation.fromEntry(entry)
	index.MixinDeclaration.fromEntry(entry)
	index.MixinName.fromEntry(entry)

	if index.isDeclaration() && index.MixinName.Name == nil {
		return fmt.Errorf("mailformed data - declaration without name: offset %d", index.Offset)
	}

	return nil
}

func newHierarchyItemFromIndex(index *IndexEntry) *HierarchyItem {
	item := HierarchyItem{}
	item.Set(index)

	item.MixinDeclaration.fromEntry(index.Entry)
	item.MixinDeclLocation.fromEntry(index.Entry)

	return &item
}

type MetaDbBuilder struct {
	// in Data
	data *dwarfy.Data

	index map[dwarfy.Offset]*IndexEntry

	names map[string]int // to calculate the next id

	// what is getting built
	metaDb *MetaDb
}

func NewMetaDb(data *dwarfy.Data) (*MetaDb, error) {
	if data == nil {
		return nil, fmt.Errorf("data pointer can't be nil")
	}
	builder := MetaDbBuilder{
		data:  data,
		index: map[dwarfy.Offset]*IndexEntry{},
		names: map[string]int{},
		metaDb: &MetaDb{
			BaseTypes:        map[CommonId]*BaseType{},
			EnumerationTypes: map[CommonId]*EnumerationType{},
			ArrayTypes:       map[CommonId]*ArrayType{},
			PointerTypes:     map[CommonId]*PointerType{},
			ReferenceTypes:   map[CommonId]*ReferenceType{},
			UnspecifiedTypes: map[CommonId]*UnspecifiedType{},
			SubroutineTypes:  map[CommonId]*SubroutineType{},
			QualTypes:        map[CommonId]*QualType{},
			Typedefs:         map[CommonId]*Typedef{},
			ClassTypes:       map[CommonId]*StructType{},
			StructTypes:      map[CommonId]*StructType{},
			UnionTypes:       map[CommonId]*StructType{},

			Variables:    map[CommonId]*Variable{},
			Subprograms:  map[CommonId]*Subprogram{},
			LexBlocks:    map[CommonId]*LexBlock{},
			Namespaces:   map[CommonId]*Namespace{},
			CompileUnits: map[CommonId]*CompileUnit{},
		}}
	err := builder.build()
	if err != nil {
		return nil, err
	}
	return builder.metaDb, nil
}

func (builder *MetaDbBuilder) getIndex(offset dwarfy.Offset) (*IndexEntry, bool, error) {
	entry, ok := builder.data.Entries[offset]
	if !ok {
		return nil, false, fmt.Errorf("wasn't able to find entry with offset %d", offset)
	}
	indexEntry, ok := builder.index[offset]
	if !ok { // create, init and register
		indexEntry = &IndexEntry{Offset: offset}
		indexEntry.fromEntry(entry)
		builder.index[offset] = indexEntry
	}
	return indexEntry, ok, nil
}

func (builder *MetaDbBuilder) processBaseType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	baseType := BaseType{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&baseType)
	err := baseType.fromEntry(index.Entry)
	if err != nil {
		return nil, fmt.Errorf("baseType parsing error for %d: %v", index.Offset, err)
	}
	if baseType.Name == nil {
		return nil, fmt.Errorf("baseType must have Name. %d doesn't have it", index.Offset)
	}
	return index, nil
}

func (builder *MetaDbBuilder) processEnumerationType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	enumerationType := EnumerationType{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&enumerationType)
	enumerationType.fromEntry(index.Entry)

	for _, childOffset := range index.Entry.Children {
		childEntry, ok := builder.data.Entries[childOffset]
		if !ok {
			return nil, fmt.Errorf("wasn't able to find child entry with offset %d", childOffset)
		}
		if childEntry.Tag != "Enumerator" {
			return nil, fmt.Errorf("found %s as child of EnumerationType with offset %d", childEntry.Tag, childOffset)
		}
		name, ok := childEntry.Val("Name").(string)
		if !ok {
			return nil, fmt.Errorf("enumerator with offset %d doesn't have name", childOffset)

		}
		constValue, ok := childEntry.Val("ConstValue").(int64)
		if !ok {
			return nil, fmt.Errorf("enumerator with offset %d doesn't have name", childOffset)

		}
		enumerationType.Enumerators = append(enumerationType.Enumerators, struct {
			Key string
			Val int64
		}{Key: name, Val: constValue})
	}
	return index, nil
}

func (builder *MetaDbBuilder) processArrayType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	var pType ICommon
	var pTypeIndex *IndexEntry
	var err error
	inType, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		pTypeIndex, err = builder.processTypeEntry(inType)
		if err != nil {
			return nil, err
		}
		pType = pTypeIndex
	} // else - void*

	if len(index.Entry.Children) == 0 {
		// this is possible for flexible 1-d e.g. "array[]"" - create it
		arrayType := ArrayType{Count: -1}
		arrayType.Type.Set(pTypeIndex)
		pType = &arrayType

	} else {
		for child := range index.Entry.Children {
			// going bottom-up
			childOffset := index.Entry.Children[len(index.Entry.Children)-1-child]
			indx, _, err := builder.getIndex(childOffset)
			if err != nil {
				return nil, err
			}
			if indx.Entry.Tag != "SubrangeType" {
				// the spec claims there can be also EnumerationType - no idea how to handle that
				return nil, fmt.Errorf("found %s as child of ArrayType with offset %d", indx.Entry.Tag, childOffset)
			}
			arrayType := ArrayType{Count: -1, Level: int64(child)}
			arrayType.Type.Set(pTypeIndex)
			lowerBound, ok := indx.Entry.Val("LowerBound").(int64)
			if ok {
				arrayType.LowerBound = &lowerBound
			}
			count, ok := indx.Entry.Val("Count").(int64)
			if ok {
				arrayType.Count = count
			} else {
				upperBound, ok := indx.Entry.Val("UpperBound").(int64)
				if ok {
					arrayType.Count = upperBound + 1
					if arrayType.LowerBound != nil {
						arrayType.Count -= *arrayType.LowerBound
					}
				}
			}

			// set parent for the lower level to current level
			if child != 0 {
				lowerArray := pType.(*ArrayType)
				if child == len(index.Entry.Children)-1 {
					lowerArray.Parents = append(lowerArray.Parents, &CommonLink{p: index})
				} else {
					lowerArray.Parents = append(lowerArray.Parents, &CommonLink{p: indx})
				}
			}

			// don't store the toplevel here
			if child < len(index.Entry.Children)-1 {
				indx.Tag = "ArrayType" // change Tag of the index, since we're storing here array
				indx.cu = cu
				indx.Set(&arrayType)
			}
			// ready for the next level
			pType = &arrayType
			pTypeIndex = indx
		}
	}

	arrayType := pType.(*ArrayType)
	arrayType.Parents = append(arrayType.Parents, &CommonLink{p: parent})
	arrayType.fromEntry(index.Entry)

	strideSize, ok := index.Entry.Val("StrideSize").(int64)
	if ok {
		arrayType.StrideBitSize = &strideSize
	}
	index.cu = cu
	index.Set(arrayType)
	return index, nil
}

func (builder *MetaDbBuilder) processMixinFunc(
	funcType *MixinFunc,
	entry *dwarfy.Entry,
	f func(i int, childOffset dwarfy.Offset, childEntry *dwarfy.Entry) error) error {
	// Subroutine type.  (DWARF v2 §5.7)
	// Attributes:
	//	AttrType: type of return value if any
	//	AttrName: possible name of type
	//	AttrPrototyped: whether used ANSI C prototype [ignored]
	// Children:
	//	TagFormalParameter: typed parameter
	//		AttrType: type of parameter
	//	TagUnspecifiedParameter: final ...
	ttype, ok := entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		val, err := builder.processTypeEntry(ttype)
		if err != nil {
			return err
		}
		funcType.Type.Set(val)
	} else {
		funcType.Type.Set(nil)
	}

	hadUnspecifiedParam := false
	for i, childOffset := range entry.Children {
		childEntry, ok := builder.data.Entries[childOffset]
		if !ok {
			return fmt.Errorf("wasn't able to find child entry with offset %d", childOffset)
		}
		if hadUnspecifiedParam && (childEntry.Tag == "FormalParameter" ||
			childEntry.Tag == "UnspecifiedParameters" ||
			childEntry.Tag == "TemplateTypeParameter") {
			return fmt.Errorf("got some exter param with offset %d after unspecifice param", childOffset)
		}
		switch childEntry.Tag {
		case "UnspecifiedParameters":
			hadUnspecifiedParam = true
			fallthrough
		case "FormalParameter":
			formalParameter := FuncParam{}
			if childEntry.Tag == "UnspecifiedParameters" {
				unspec := true
				formalParameter.UnspecifiedParam = &unspec
			}
			formalParameter.fromEntry(childEntry)
			xttype, ok := childEntry.Val("Type").(dwarfy.Offset)
			if ok {
				// make sure that the underlaying type is created
				val, err := builder.processTypeEntry(xttype)
				if err != nil {
					return err
				}
				formalParameter.Type.Set(val)
			} else {
				// it's ok for UnspecifiedParameters
				formalParameter.Type.Set(nil)
			}
			funcType.Param = append(funcType.Param, &formalParameter)
		case "CallSite": // Met on Windows and isn't needed for now
		case "TemplateTypeParameter":
			//TODO: c++ only (check CU?)
		default:
			if f != nil {
				err := f(i, childOffset, childEntry)
				if err != nil {
					return err
				}
			}
		}
	}
	return nil
}

func (builder *MetaDbBuilder) processSubroutineType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	subroutineType := SubroutineType{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&subroutineType)
	subroutineType.fromEntry(index.Entry)
	err := builder.processMixinFunc(&subroutineType.MixinFunc, index.Entry, nil)
	if err != nil {
		return nil, err
	}
	return index, nil
}

func (builder *MetaDbBuilder) processPointerType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	pointerType := PointerType{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&pointerType)
	pointerType.fromEntry(index.Entry)

	ttype, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		val, err := builder.processTypeEntry(ttype)
		if err != nil {
			return nil, err
		}
		pointerType.Type.Set(val)
	} else {
		pointerType.Type.Set(nil)
	}
	return index, nil
}

func (builder *MetaDbBuilder) processReferenceType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	referenceType := ReferenceType{Tag: string(index.Entry.Tag), CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&referenceType)
	referenceType.fromEntry(index.Entry)

	ttype, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		val, err := builder.processTypeEntry(ttype)
		if err != nil {
			return nil, err
		}
		referenceType.Type.Set(val)
	} else {
		referenceType.Type.Set(nil)
	}
	return index, nil
}

func (builder *MetaDbBuilder) processUnspecifiedType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	unspecifiedType := UnspecifiedType{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&unspecifiedType)
	unspecifiedType.fromEntry(index.Entry)
	return index, nil
}

func (builder *MetaDbBuilder) processQualType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	quialType := QualType{Qual: string(index.Entry.Tag), CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&quialType)
	quialType.fromEntry(index.Entry)

	ttype, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		val, err := builder.processTypeEntry(ttype)
		if err != nil {
			return nil, err
		}
		quialType.Type.Set(val)
	} else {
		quialType.Type.Set(nil)
	}
	return index, nil
}

func (builder *MetaDbBuilder) processTypedef(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	typedef := Typedef{CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&typedef)
	typedef.fromEntry(index.Entry)
	if typedef.Name == nil {
		return nil, fmt.Errorf("Typedef must have Name. %d doesn't have it", index.Offset)
	}
	ttype, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		ttypeIndex, err := builder.processTypeEntry(ttype)
		if err != nil {
			return nil, err
		}
		typedef.Type.Set(ttypeIndex)
	} else {
		typedef.Type.Set(nil)

	}
	return index, nil
}

func (builder *MetaDbBuilder) processStructType(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	structType := StructType{Tag: string(index.Entry.Tag), CommonType: CommonType{Parents: []*CommonLink{{p: parent}}}}
	index.cu = cu
	index.Set(&structType)
	structType.fromEntry(index.Entry)

	for _, childOffset := range index.Entry.Children {
		childEntry, ok := builder.data.Entries[childOffset]
		if !ok {
			return nil, fmt.Errorf("wasn't able to find child entry with offset %d", childOffset)
		}
		switch childEntry.Tag {
		case "Member":
			structField := StructField{}

			// read member type
			ttype, ok := childEntry.Val("Type").(dwarfy.Offset)
			if !ok {
				return nil, fmt.Errorf("couldn't read Type attribute for member offset %d", childOffset)
			}
			// make sure that the underlaying type is created
			val, err := builder.processTypeEntry(ttype)
			if err != nil {
				return nil, err
			}
			structField.Type.Set(val)

			// read optional params
			name, ok := childEntry.Val("Name").(string)
			if ok {
				structField.Name = &name
			}
			byteSize, ok := childEntry.Val("ByteSize").(int64)
			if ok {
				structField.ByteSize = &byteSize
			}
			bitOffset, ok := childEntry.Val("BitOffset").(int64)
			if ok {
				structField.BitOffset = &bitOffset
				if structField.ByteSize == nil {
					return nil, fmt.Errorf("DWARF3 BitOffset field must go with ByteSize. which isn't true for member offset %d", childOffset)
				}
			}
			dataBitOffset, ok := childEntry.Val("DataBitOffset").(int64)
			if ok {
				structField.DataBitOffset = &dataBitOffset
			}
			bitSize, ok := childEntry.Val("BitSize").(int64)
			if ok {
				structField.BitSize = &bitSize
			}

			// read member offset - mandatory for structs if fields are not set via dataBitOffset
			structField.ByteOffset, ok = childEntry.Val("DataMemberLoc").(int64)
			if !ok && index.Entry.Tag != "UnionType" && structField.DataBitOffset == nil {
				return nil, fmt.Errorf("couldn't read DataMemberLoc attribute for member offset %d", childOffset)
			}

			structType.Fields = append(structType.Fields, &structField)
		case "TemplateTypeParameter":
		default:
			indx, err := builder.processEntry(cu, index, childOffset)
			if err != nil {
				return nil, err
			}
			structType.Hierarchy = append(structType.Hierarchy, newHierarchyItemFromIndex(indx))
		}

	}
	return index, nil
}

func (builder *MetaDbBuilder) processVariable(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	variable := Variable{CommonObj: CommonObj{Parent: CommonLink{p: parent}}}
	index.cu = cu
	index.Set(&variable)
	variable.fromEntry(index.Entry)

	ttype, ok := index.Entry.Val("Type").(dwarfy.Offset)
	if ok {
		// make sure that the underlaying type is created
		val, err := builder.processTypeEntry(ttype)
		if err != nil {
			return nil, err
		}
		variable.Type.Set(val)
	} else {
		variable.Type.Set(nil)
	}
	builder.appendDb(&variable)
	return index, nil
}

func (builder *MetaDbBuilder) processLexBlock(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	block := LexBlock{Parent: CommonLink{p: parent}}
	index.cu = cu
	index.Set(&block)
	block.fromEntry(index.Entry)

	for _, childOffset := range index.Entry.Children {
		indx, err := builder.processEntry(cu, index, childOffset)
		if err != nil {
			return nil, err
		}
		if indx != nil {
			block.Hierarchy = append(block.Hierarchy, newHierarchyItemFromIndex(indx))
		}
	}
	builder.appendDb(&block)
	return index, nil
}

func (builder *MetaDbBuilder) processSubprogram(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	subprogram := Subprogram{CommonObj: CommonObj{Parent: CommonLink{p: parent}}}
	index.cu = cu
	index.Set(&subprogram)
	subprogram.fromEntry(index.Entry)
	err := builder.processMixinFunc(&subprogram.MixinFunc, index.Entry,
		func(i int, childOffset dwarfy.Offset, childEntry *dwarfy.Entry) error {
			indx, err := builder.processEntry(cu, index, childOffset)
			if err != nil {
				return err
			}
			if indx != nil {
				subprogram.Hierarchy = append(subprogram.Hierarchy, newHierarchyItemFromIndex(indx))
			}
			return nil
		})
	if err != nil {
		return nil, err
	}
	builder.appendDb(&subprogram)
	return index, nil
}

func (builder *MetaDbBuilder) processNamespace(cu *CompileUnit, parent ICommon, index *IndexEntry) (*IndexEntry, error) {
	namespace := Namespace{Parent: CommonLink{p: parent}}
	index.cu = cu
	index.Set(&namespace)
	namespace.fromEntry(index.Entry)
	for _, childOffset := range index.Entry.Children {
		childEntry, ok := builder.data.Entries[childOffset]
		if !ok {
			return nil, fmt.Errorf("wasn't able to find child entry with offset %d", childOffset)
		}
		switch childEntry.Tag {
		case "ImportedModule":
			fallthrough
		case "ImportedDeclaration":
			item := CommonLink{}
			importOffset, ok := childEntry.Val("Import").(dwarfy.Offset)
			if ok {
				// make sure that the underlaying type is created
				val, err := builder.processEntry(cu, index, importOffset)
				if err != nil {
					return nil, err
				}
				item.Set(val)
			} else {
				item.Set(nil)
			}
			switch childEntry.Tag {
			case "ImportedModule":
				namespace.ImportedModules = append(namespace.ImportedModules, &item)
			case "ImportedDeclaration":
				namespace.ImportedDeclarations = append(namespace.ImportedDeclarations, &item)
			}
		default:
			indx, err := builder.processEntry(cu, index, childOffset)
			if err != nil {
				return nil, err
			}
			namespace.Hierarchy = append(namespace.Hierarchy, newHierarchyItemFromIndex(indx))
		}
	}
	builder.appendDb(&namespace)
	return index, nil
}

func (builder *MetaDbBuilder) processCompileUnit(index *IndexEntry) (*IndexEntry, error) {
	compileUnit := CompileUnit{}
	index.cu = &compileUnit
	index.Set(&compileUnit)
	var ok bool
	compileUnit.Name, ok = index.Entry.Val("Name").(string)
	if !ok {
		return nil, fmt.Errorf("CompileUnit must have a Name")
	}
	lang, ok := index.Entry.Val("Language").(int64)
	if ok {
		l := dwarfy.Language(lang)
		lf := l.Family()
		compileUnit.Lang = &l
		compileUnit.LangFamily = &lf
	}
	if index.Entry.DeclFiles != nil {
		for _, fileName := range *index.Entry.DeclFiles {
			absPath, err := filepath.Abs(fileName)
			if err != nil {
				// we don't care about files which we can't convert to absolute path (they probably were compiled somewhere else)
				compileUnit.DeclFiles = append(compileUnit.DeclFiles, fileName)
				continue
			}
			compileUnit.DeclFiles = append(compileUnit.DeclFiles, absPath)
		}
	}

	for _, offset := range index.Entry.Children {
		childEntry, ok := builder.data.Entries[offset]
		if !ok {
			return nil, fmt.Errorf("wasn't able to find child entry with offset %d", offset)
		}
		switch childEntry.Tag {
		case "ImportedModule":
			fallthrough
		case "ImportedDeclaration":
			item := CommonLink{}
			importOffset, ok := childEntry.Val("Import").(dwarfy.Offset)
			if ok {
				// make sure that the underlaying type is created
				val, err := builder.processEntry(&compileUnit, index, importOffset)
				if err != nil {
					return nil, err
				}
				item.Set(val)
			} else {
				item.Set(nil)
			}
			switch childEntry.Tag {
			case "ImportedModule":
				compileUnit.ImportedModules = append(compileUnit.ImportedModules, &item)
			case "ImportedDeclaration":
				compileUnit.ImportedDeclarations = append(compileUnit.ImportedDeclarations, &item)
			}
		default:
			indx, err := builder.processEntry(&compileUnit, index, offset)
			if err != nil {
				return nil, err
			}
			compileUnit.Hierarchy = append(compileUnit.Hierarchy, newHierarchyItemFromIndex(indx))
		}
	}
	builder.appendDb(&compileUnit)
	return index, nil
}

func (builder *MetaDbBuilder) processTypeEntry(offset dwarfy.Offset) (*IndexEntry, error) {
	index, _, err := builder.getIndex(offset)
	if err != nil {
		return nil, fmt.Errorf("getIndex %d returned: %v", offset, err)
	}
	if !isTypeTag(index.Entry.Tag) {
		return nil, fmt.Errorf("getIndex %d tag is %s which isn't type tag", offset, index.Entry.Tag)
	}
	return index, nil
}

func (builder *MetaDbBuilder) processEntry(cu *CompileUnit, parent ICommon, offset dwarfy.Offset) (*IndexEntry, error) {
	index, _, err := builder.getIndex(offset)
	if err != nil {
		return nil, fmt.Errorf("newIndex %d returned: %v", offset, err)
	}

	switch index.Tag {
	case "BaseType":
		_, err = builder.processBaseType(cu, parent, index)
	case "EnumerationType":
		_, err = builder.processEnumerationType(cu, parent, index)
	case "ArrayType":
		_, err = builder.processArrayType(cu, parent, index)
	case "PointerType":
		_, err = builder.processPointerType(cu, parent, index)
	case "ReferenceType": // C++ specific
		fallthrough
	case "RvalueReferenceType": // C++ specific
		_, err = builder.processReferenceType(cu, parent, index)
	case "UnspecifiedType": // C++ specific
		_, err = builder.processUnspecifiedType(cu, parent, index)
	case "SubroutineType":
		_, err = builder.processSubroutineType(cu, parent, index)
	case "Typedef":
		_, err = builder.processTypedef(cu, parent, index)
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
		_, err = builder.processQualType(cu, parent, index)
	case "ClassType":
		fallthrough
	case "StructType":
		fallthrough
	case "UnionType":
		_, err = builder.processStructType(cu, parent, index)
	case "Variable":
		_, err = builder.processVariable(cu, parent, index)
	case "LexDwarfBlock":
		_, err = builder.processLexBlock(cu, parent, index)
	case "Subprogram":
		_, err = builder.processSubprogram(cu, parent, index)
	case "Namespace":
		_, err = builder.processNamespace(cu, parent, index)
	/*found on windows: mingw64*/
	case "Label": // goto label???
		fallthrough
	case "CallSite": // kind of place from which function is called. we don't track that.
		fallthrough
	case "InlinedSubroutine": // place inside Subroutine or lexblock where other fn called. we don't track
		return nil, nil
	default:
		return nil, fmt.Errorf("tag %s unsupported by processEntry", index.Tag)
	}
	if err != nil {
		return nil, err
	}
	return index, nil
}

func (builder *MetaDbBuilder) build() error {
	// take entry0 in Data and go deeper
	startOffset := dwarfy.Offset(0)
	entry, ok := builder.data.Entries[startOffset]
	if !ok {
		return fmt.Errorf("wasn't able to find entry with offset %d", startOffset)
	}
	for _, offset := range entry.Children {
		index, ok, err := builder.getIndex(offset)
		if err != nil {
			return fmt.Errorf("getIndex %d returned: %v", offset, err)
		}
		if ok {
			return fmt.Errorf("getIndex %d returned already existing index", offset)
		}
		if index.Tag != "CompileUnit" {
			return fmt.Errorf("mailformed input data: expected CompileUnit and found %s", index.Tag)
		}
		_, err = builder.processCompileUnit(index)
		if err != nil {
			return err
		}
	}
	err := builder.deduplicateTypes()
	if err != nil {
		return err
	}
	builder.metaDb.streamline()
	return nil
}

func (builder *MetaDbBuilder) deduplicateTypes() error {
	headerSignatures := map[string]*IndexEntry{}
	signatures := map[string]map[string]*IndexEntry{}
	// go via all indexes, find only types
	for _, indx := range builder.index {
		if indx.p == nil {
			continue
		}
		if isTypeTag(indx.Tag) {
			// skip children of duplicated entries
			pIndx, _, err := builder.getIndex(*indx.Entry.Parent)
			if err != nil {
				return err
			}
			if pIndx.isDuplicate {
				indx.isDuplicate = true
				continue
			}

			hs, err := indx.p.Signature(false)
			if err != nil {
				return err
			}
			if indx.isDeclaration() { // declarations can be deduplicated by header signature
				found, ok := headerSignatures[hs]
				if ok {
					//merge parents
					icmnt := indx.GetCommonType()
					fcmnt := found.GetCommonType()
					if icmnt == nil || fcmnt == nil {
						return fmt.Errorf("coun't merge parents - one of the object isn't type")
					}
					fcmnt.Parents = append(fcmnt.Parents, icmnt.Parents...)
					//dedup
					indx.isDuplicate = true
					indx.p = found.p
					continue
				}
				headerSignatures[hs] = indx
				builder.appendDb(indx.p)
				continue
			}
			// non-declaration... hmm.. use comparable signature (may want to rework this later if needed)
			cs, err := indx.p.Signature(true)
			if err != nil {
				return err
			}
			foundMap, ok := signatures[hs]
			if ok {
				found, ok := foundMap[cs]
				if ok {
					//merge parents
					icmnt := indx.GetCommonType()
					fcmnt := found.GetCommonType()
					if icmnt == nil || fcmnt == nil {
						return fmt.Errorf("couldn't merge parents - one of the objects isn't type")
					}
					fcmnt.Parents = append(fcmnt.Parents, icmnt.Parents...)
					//dedup
					indx.isDuplicate = true
					indx.p = found.p
					continue
				}

				if indx.Entry.Tag != "BaseType" { /* developer doesn't define base types. Clang can generate Complex base type with different sizes */
					// warning - we have several types with the same name, but different structure
					emsg := fmt.Sprintf("WARNING:\n%s: %s doesn't match with previous definition of %s from",
						indx.MixinDeclLocation.toString(indx.cu), cs, hs)
					for fcs, found := range foundMap {
						emsg += fmt.Sprintf("\n%s: %s", found.MixinDeclLocation.toString(found.cu), fcs)
					}
					log.Print(emsg)
				}

				foundMap[cs] = indx
				builder.appendDb(indx.p)
				continue

			}
			signatures[hs] = map[string]*IndexEntry{cs: indx}
			builder.appendDb(indx.p)
		} //else {
		// TODO: dedup for hierarchy objects... - declarations of variables and subroutines
		// }
	}
	return nil
}

func (builder *MetaDbBuilder) commonIdFromICommon(ic ICommon) CommonId {
	prefix := ""
	name := ""
	switch x := ic.(type) {
	case *BaseType:
		prefix = ""
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *EnumerationType:
		prefix = "e"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *ArrayType:
		prefix = "a"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *PointerType:
		prefix = "p"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *ReferenceType:
		prefix = "ref"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *UnspecifiedType:
		prefix = "unspec"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *SubroutineType:
		prefix = "sbrtn"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *QualType:
		prefix = "qual"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *Typedef:
		prefix = "t"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *StructType:
		switch x.Tag {
		case "UnionType":
			prefix = "u"
			if x.Name != nil {
				name = xstrings.ToSnakeCase(*x.Name)
			}
		case "StructType":
			prefix = "s"
			if x.Name != nil {
				name = xstrings.ToSnakeCase(*x.Name)
			}
		case "ClassType":
			prefix = "c"
			if x.Name != nil {
				name = xstrings.ToSnakeCase(*x.Name)
			}
		}
	case *Variable:
		prefix = "var"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *Subprogram:
		prefix = "fn"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		} else {
			// TODO: C++ has actual implementaiton of abstract functions
			name = "tbd"
		}
	case *LexBlock:
		prefix = "blk"
	case *Namespace:
		prefix = "ns"
		if x.Name != nil {
			name = xstrings.ToSnakeCase(*x.Name)
		}
	case *CompileUnit:
		prefix = "cu"
	}
	if name != "" {
		if prefix != "" {
			prefix += "_"
		}
		prefix += name
	}
	cnt, ok := builder.names[prefix]
	if !ok {
		cnt = 0
	} else {
		cnt++
	}
	builder.names[prefix] = cnt

	return CommonId(fmt.Sprintf("%s_%04d", prefix, cnt))
}

func (builder *MetaDbBuilder) appendDb(ic ICommon) {
	id := builder.commonIdFromICommon(ic)
	switch x := ic.(type) {
	case *BaseType:
		x.id = id
		builder.metaDb.BaseTypes[id] = x
	case *EnumerationType:
		x.id = id
		builder.metaDb.EnumerationTypes[id] = x
	case *ArrayType:
		x.id = id
		builder.metaDb.ArrayTypes[id] = x
	case *PointerType:
		x.id = id
		builder.metaDb.PointerTypes[id] = x
	case *ReferenceType:
		x.id = id
		builder.metaDb.ReferenceTypes[id] = x
	case *UnspecifiedType:
		x.id = id
		builder.metaDb.UnspecifiedTypes[id] = x
	case *SubroutineType:
		x.id = id
		builder.metaDb.SubroutineTypes[id] = x
	case *QualType:
		x.id = id
		builder.metaDb.QualTypes[id] = x
	case *Typedef:
		x.id = id
		builder.metaDb.Typedefs[id] = x
	case *StructType:
		x.id = id
		switch x.Tag {
		case "UnionType":
			builder.metaDb.UnionTypes[id] = x
		case "StructType":
			builder.metaDb.StructTypes[id] = x
		case "ClassType":
			builder.metaDb.ClassTypes[id] = x
		}
	case *Variable:
		x.id = id
		builder.metaDb.Variables[id] = x
	case *Subprogram:
		x.id = id
		builder.metaDb.Subprograms[id] = x
	case *LexBlock:
		x.id = id
		builder.metaDb.LexBlocks[id] = x
	case *Namespace:
		x.id = id
		builder.metaDb.Namespaces[id] = x
	case *CompileUnit:
		x.id = id
		builder.metaDb.CompileUnits[id] = x
	}
}
