{{- /* generate ALL metadb early declarations */ -}}
{{- define "metac_reflect_gen.early_declarations" -}}
{{-   $metaDb := . }}
/* early declaration */
{{-   include "metac_reflect_gen.early_declaration" (dict "BaseTypes" $metaDb.BaseTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "EnumerationTypes" $metaDb.EnumerationTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "ArrayTypes" $metaDb.ArrayTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "PointerTypes" $metaDb.PointerTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "ReferenceTypes" $metaDb.ReferenceTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "UnspecifiedTypes" $metaDb.UnspecifiedTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "SubroutineTypes" $metaDb.SubroutineTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "QualTypes" $metaDb.QualTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "TypeDefs" $metaDb.Typedefs) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "ClassTypes" $metaDb.ClassTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "StructTypes" $metaDb.StructTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "UnionTypes" $metaDb.UnionTypes) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "Subprograms" $metaDb.Subprograms) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "Variables" $metaDb.Variables) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "LexBlocks" $metaDb.LexBlocks) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "Namespaces" $metaDb.Namespaces) -}}
{{-   include "metac_reflect_gen.early_declaration" (dict "ComplieUnits" $metaDb.CompileUnits) -}}
{{- end -}}
{{- /* herlper: generate metadb early declaration of 1 type */ -}}
{{- define "metac_reflect_gen.early_declaration" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$_ := $map }}
static struct metac_entry {{ $i }};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
