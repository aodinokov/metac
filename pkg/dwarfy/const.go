package dwarfy

import "strconv"

//go:generate stringer -type Encoding -trimprefix=Enc

type Encoding int64

const (
	EncVoid           Encoding = 0x0
	EncAddress        Encoding = 0x1
	EncBoolean        Encoding = 0x2
	EncComplexFloat   Encoding = 0x3
	EncFloat          Encoding = 0x4
	EncSigned         Encoding = 0x5
	EncSignedChar     Encoding = 0x6
	EncUnsigned       Encoding = 0x7
	EncUnsignedChar   Encoding = 0x8
	EncImaginaryFloat Encoding = 0x9
	EncPackedDecimal  Encoding = 0xa
	EncNumericString  Encoding = 0xb
	EncEdited         Encoding = 0xc
	EncSignedFixed    Encoding = 0xd
	EncUnsignedFixed  Encoding = 0xe
	EncDecimalFloat   Encoding = 0xf
	EncUtf            Encoding = 0x10
	EncUcs            Encoding = 0x11
	EncAscii          Encoding = 0x12
	EncLoUser         Encoding = 0x80
	EncHiUser         Encoding = 0xff
)

func (e Encoding) GoString() string {
	if e < 0 || e >= Encoding(len(_Encoding_index_0)-1) {
		return "dwarf.Encoding(" + strconv.FormatInt(int64(e), 10) + ")"
	}
	return "dwarf." + e.String()
}

//go:generate stringer -type Language -trimprefix=Lang

type Language int64

const (
	LangC89            Language = 0x0001
	LangC              Language = 0x0002
	LangAda83          Language = 0x0003
	LangC_plus_plus    Language = 0x0004
	LangCobol74        Language = 0x0005
	LangCobol85        Language = 0x0006
	LangFortran77      Language = 0x0007
	LangFortran90      Language = 0x0008
	LangPascal83       Language = 0x0009
	LangModula2        Language = 0x000a
	LangJava           Language = 0x000b
	LangC99            Language = 0x000c
	LangAda95          Language = 0x000d
	LangFortran95      Language = 0x000e
	LangPLI            Language = 0x000f
	LangObjC           Language = 0x0010
	LangObjC_plus_plus Language = 0x0011
	LangUPC            Language = 0x0012
	LangD              Language = 0x0013
	LangPython         Language = 0x0014
	LangOpenCL         Language = 0x0015
	LangGo             Language = 0x0016
	LangModula3        Language = 0x0017
	LangHaskell        Language = 0x0018
	LangC_plus_plus_03 Language = 0x0019
	LangC_plus_plus_11 Language = 0x001a
	LangOCaml          Language = 0x001b
	LangRust           Language = 0x001c
	LangC11            Language = 0x001d
	LangSwift          Language = 0x001e
	LangJulia          Language = 0x001f
	LangDylan          Language = 0x0020
	LangC_plus_plus_14 Language = 0x0021
	LangFortran03      Language = 0x0022
	LangFortran08      Language = 0x0023
	LangRenderScript   Language = 0x0024
	LangBLISS          Language = 0x0025
	Langlo_user        Language = 0x8000
	Langhi_user        Language = 0xffff
)

func (l Language) GoString() string {
	if l < 0 || l >= Language(len(_Language_index_0)-1) {
		return "dwarf.Language(" + strconv.FormatInt(int64(l), 10) + ")"
	}
	return "dwarf." + l.String()
}

func (l Language) Family() string {
	switch l {
	case LangC89:
		fallthrough
	case LangC:
		fallthrough
	case LangC99:
		fallthrough
	case LangC11:
		return "C"

	case LangAda83:
		fallthrough
	case LangAda95:
		return "Ada"

	case LangC_plus_plus:
		fallthrough
	case LangC_plus_plus_03:
		fallthrough
	case LangC_plus_plus_11:
		fallthrough
	case LangC_plus_plus_14:
		return "C_plus_plus"

	case LangCobol74:
		fallthrough
	case LangCobol85:
		return "Cobol"

	case LangFortran77:
		fallthrough
	case LangFortran90:
		fallthrough
	case LangFortran95:
		fallthrough
	case LangFortran03:
		fallthrough
	case LangFortran08:
		return "Fortran"

	}
	return l.String()
}
