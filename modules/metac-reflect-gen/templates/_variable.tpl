{{- /* generate variable declarations */ -}}
{{- define "metac_reflect_gen.variable" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_variable,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.parent" $v) }}
    .variable_info = {
{{-          indent 8 (include "metac_reflect_gen.type" $v.Type) -}}
{{-          indent 8 (include "metac_reflect_gen.obj_mixin" $v) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}