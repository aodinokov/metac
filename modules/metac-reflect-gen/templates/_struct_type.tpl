{{- /* generate sctuct,union,class declarations */ -}}
{{- define "metac_reflect_gen.struct_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map -}}
{{-          $declaraion := false -}}
{{-          with $v.ByteSize -}}
{{-            $sz := toJson . }}
// size {{ $sz }}
static metac_flag_t {{ $i }}_va_arg(struct va_list_container *p_va_list_container, void * buf) {
    if (p_va_list_container != NULL && buf != NULL) {
{{- /* 

This code is done this way in order to avoid an issue faced with Windows: 
if the structure has size 1,2,4 or 8 (potentially 16 for some platforms)
va_arg(p_va_list_container->parameters, char[<size>]) just fails.
but it works for other sizes.
Also it workes ok for Linux and Mac. for now only Windows has this issue.
the WA demonstrated here works ok for our tests.

In general it appears that va_list is very specific to platform
and has many issues when you try to use it for structures (not pointers).
*/ -}}
{{-            if eq "1" $sz }}
        uint8_t data = va_arg(p_va_list_container->parameters, int);
        memcpy(buf, &data, {{ . }});
        return 1;
{{-            else if eq "2" $sz }}
        uint16_t data = va_arg(p_va_list_container->parameters, int);
        memcpy(buf, &data, {{ . }});
        return 1;
{{-            else if eq "4" $sz }}
        uint32_t data = va_arg(p_va_list_container->parameters, uint32_t);
        memcpy(buf, &data, {{ . }});
        return 1;
{{-            else if eq "8" $sz }}
        uint64_t data = va_arg(p_va_list_container->parameters, uint64_t);
        memcpy(buf, &data, {{ . }});
        return 1;
{{-            else }}
        long long data;
        if (sizeof(data) == {{ . }}) {
            data = va_arg(p_va_list_container->parameters, long long);
            memcpy(buf, &data, {{ . }});
            return 1;
        }
        void * p = (void*) va_arg(p_va_list_container->parameters, char[{{ . }}]);
        if (p == NULL) {
            return 0;
        }
        memcpy(buf, p, {{ . }});
        return 1;
{{-            end }}
    }
    return 0;
}
{{-          end }}
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
{{-            indent 8 (include "metac_reflect_gen.byte_size_mixin" $v) -}}
{{-            indent 8 (include "metac_reflect_gen.alignment_mixin" $v) }}
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
{{-                with $val.Alignment }}
                    .p_alignment = (metac_size_t[]){ {{ . }},},
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
