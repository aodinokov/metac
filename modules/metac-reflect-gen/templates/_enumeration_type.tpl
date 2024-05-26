{{- /* generate enum declarations */ -}}
{{- define "metac_reflect_gen.enumeration_type" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_enumeration_type,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .enumeration_type_info = {
{{-          indent 8 (include "metac_reflect_gen.byte_size_mixin" $v) }}
        .enumerators_count = {{ len $v.Enumerators }},
        .enumerators = (struct metac_type_enumerator_info[]){
{{-          range $_,$x := $v.Enumerators }}
            {.name = {{ quote $x.Key }}, .const_value = {{ $x.Val }}},
{{-          end }}
        },
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}