{{- /* generate namespace declarations */ -}}
{{- define "metac_reflect_gen.namespace" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_namespace,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.parent" $v) -}}
    .namespace_info = {
{{-          indent 8 (include "metac_reflect_gen.import" (tuple "imported_modules" $v.ImportedModules)) }}
{{-          indent 8 (include "metac_reflect_gen.import" (tuple "imported_declarations" $v.ImportedDeclarations)) }}
{{-          indent 8 (include "metac_reflect_gen.hierarchy" $v.Hierarchy) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}