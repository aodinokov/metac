{{- /* generate ALL metadb early declarations */ -}}
{{- define "metac_reflect_gen.actual_data" -}}
{{-   $metaDb := . }}
/* actual data */
{{-   include "metac_reflect_gen.base_type" (dict "BaseTypes" $metaDb.BaseTypes) -}}
{{-   include "metac_reflect_gen.enumeration_type" (dict "EnumerationTypes" $metaDb.EnumerationTypes) -}}
{{-   include "metac_reflect_gen.array_type" (dict "ArrayTypes" $metaDb.ArrayTypes) -}}
{{-   include "metac_reflect_gen.pointer_type" (dict "PointerTypes" $metaDb.PointerTypes) -}}
{{-   include "metac_reflect_gen.reference_type" (dict "ReferenceTypes" $metaDb.ReferenceTypes) -}}
{{-   include "metac_reflect_gen.unspecified_type" (dict "UnspecifiedTypes" $metaDb.UnspecifiedTypes) -}}
{{-   include "metac_reflect_gen.subroutine_type" (dict "SubroutineTypes" $metaDb.SubroutineTypes) -}}
{{-   include "metac_reflect_gen.qual_type" (dict "QualTypes" $metaDb.QualTypes) -}}
{{-   include "metac_reflect_gen.typedef" (dict "TypeDefs" $metaDb.Typedefs) -}}
{{-   include "metac_reflect_gen.struct_type" (dict "ClassTypes" $metaDb.ClassTypes) -}}
{{-   include "metac_reflect_gen.struct_type" (dict "StructTypes" $metaDb.StructTypes) -}}
{{-   include "metac_reflect_gen.struct_type" (dict "UnionTypes" $metaDb.UnionTypes) -}}
{{-   include "metac_reflect_gen.subprogram" (dict "Subprograms" $metaDb.Subprograms) -}}
{{-   include "metac_reflect_gen.variable" (dict "Variables" $metaDb.Variables) -}}
{{-   include "metac_reflect_gen.lex_block" (dict "LexBlocks" $metaDb.LexBlocks) -}}
{{-   include "metac_reflect_gen.namespace" (dict "Namespaces" $metaDb.Namespaces) -}}
{{-   include "metac_reflect_gen.compile_unit" (dict "ComplieUnits" $metaDb.CompileUnits) -}}
{{- end -}}
