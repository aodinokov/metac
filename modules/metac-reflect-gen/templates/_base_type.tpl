{{- /* generate base type declarations */ -}}
{{- define "metac_reflect_gen.base_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_base_type,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .base_type_info = {
{{- /*       with $v.Name clang has some tricky baseTypes, e.g. __ARRAY_SIZE_TYPE__ which doesn't work with sizeof }}
        .byte_size = sizeof({{ $v.Name }}),
{{-          end */ -}}
{{-          indent 8 (include "metac_reflect_gen.byte_size_mixin" $v) }}
        .encoding = METAC_ENC_{{ snakecase $v.Encoding }},
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}