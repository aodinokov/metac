{{- /* generate array declarations */ -}}
{{- define "metac_reflect_gen.array_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_array_type,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .array_type_info = {
        .count = {{ $v.Count }},
        .level = {{ $v.Level }},
{{-          with $v.LowerBound }}
        .lower_bound = {{ . }},
{{-          end }}
{{-          with $v.StrideBitSize }}
        .p_stride_bit_size = (metac_bit_size_t[]){ { {{ . }} }, },
{{-          end }}
{{-          if ne $v.Type.Id "" }}
        .type = &{{ $v.Type.Id }},
{{-          end }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}