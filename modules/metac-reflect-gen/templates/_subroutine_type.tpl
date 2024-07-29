{{- /* generate subroutine declarations */ -}}
{{- define "metac_reflect_gen.subroutine_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_subroutine_type,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .subroutine_type_info = {
{{-          indent 8 (include "metac_reflect_gen.type" $v.Type) }}
        .parameters_count = {{ len $v.Param }},
        .parameters = (struct metac_entry[]){
{{-          range $_,$val := $v.Param }}
            {
                .kind = METAC_KND_func_parameter,
{{-                indent 20 (include "metac_reflect_gen.name_mixin" $val) }}
                .parents_count = 1,
                .parents = (metac_entry_t*[]){ &{{ $i }}, },
                .func_parameter_info = {
{{-            with $val.UnspecifiedParam -}}
{{-              if eq (toJson .) "true"  }}
                    .unspecified_parameters = 1,
{{-              end -}}
{{-            end }}
{{-            indent 20 (include "metac_reflect_gen.type" $val.Type) }}
                },
            },
{{-          end }}
        },
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}