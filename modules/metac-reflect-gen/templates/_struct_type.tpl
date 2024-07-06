{{- /* generate sctuct,union,class declarations */ -}}
{{- define "metac_reflect_gen.struct_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map -}}
{{-          $declaraion := false }}
static void * {{ $i }}_va_arg(va_list *p_va_list) {
{{-   with $v.ByteSize }}
    if (p_va_list != NULL) {
        return (void*) va_arg(*p_va_list, char[{{ . }}]);
    }
{{-   end }}
    return NULL;
}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_{{ snakecase $v.Tag }},
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) -}}
{{-          with $v.Declaration -}}
{{-            if eq (toJson .) "true" -}}
{{-              $declaraion = true -}}
{{-            end -}}
{{-          end -}}
{{-          if ne $declaraion true }}
    .structure_type_info = {
{{-            indent 8 (include "metac_reflect_gen.byte_size_mixin" $v) }}
        .p_va_arg_fn = {{ $i }}_va_arg,
{{-            if ne (len $v.Fields) 0 }}
        .members_count = {{ len $v.Fields }},
        .members = (struct metac_entry[]){
{{-              range $_,$val := $v.Fields }}
            {
                .kind = METAC_KND_member,
{{-                indent 16 (include "metac_reflect_gen.name_mixin" $val) }}
                .parents_count = 1,
                .parents = (metac_entry_t*[]){ &{{ $i }}, },
                .member_info = {
{{-                indent 20 (include "metac_reflect_gen.type" $val.Type) -}}
{{-                with $val.ByteSize }}
                    .p_byte_size = (metac_size_t[]){ {{ . }},},
{{-                end }}
{{-                if ne 0 $val.ByteOffset }}
                    .byte_offset = {{ $val.ByteOffset }},
{{-                end }}
{{-                with $val.DataBitOffset }}
                    .p_data_bit_offset = (metac_size_t[]){ {{ . }},},
{{-                end }}
{{-                with $val.BitOffset }}
                    .p_bit_offset = (metac_size_t[]){ {{ . }},},
{{-                end }}
{{-                with $val.BitSize }}
                    .p_bit_size = (metac_size_t[]){ {{ . }},},
{{-                end }}
                },
            },
{{-              end }}
        },
{{-            end }}
{{-            indent 8 (include "metac_reflect_gen.hierarchy" $v.Hierarchy) }}
    },
{{-          end }}
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
