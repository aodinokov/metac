{{- /* generate reference declarations. remember: depending on impl ref can be either size of the object or pointer */ -}}
{{- define "metac_reflect_gen.reference_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_{{ snakecase $v.Tag }},
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .reference_type_info = {
{{-          indent 8 (include "metac_reflect_gen.byte_size_mixin" $v) -}}
{{-          indent 8 (include "metac_reflect_gen.type" $v.Type) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}