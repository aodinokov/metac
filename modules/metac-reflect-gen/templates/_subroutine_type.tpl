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
        .parameters = (struct metac_type_subroutine_type_parameter[]){
{{-          range $_,$val := $v.Param }}
            {
{{-            with $val.UnspecifiedParam -}}
{{-              if eq (toJson .) "true"  }}
                .unspecified_parameters = 1,
{{-              end -}}
{{-            end }}
{{-            indent 16 (include "metac_reflect_gen.type" $val.Type) }}
            },
{{-          end }}
        },
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}