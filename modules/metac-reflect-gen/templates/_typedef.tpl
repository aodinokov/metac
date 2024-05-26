{{- /* generate typdefs declarations */ -}}
{{- define "metac_reflect_gen.typedef" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_typedef,
{{-          indent 4 (include "metac_reflect_gen.name_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.declaration_mixin" $v) -}}
{{-          indent 4 (include "metac_reflect_gen.type_parents" $v) }}
    .typedef_info = {
{{-          indent 8 (include "metac_reflect_gen.type" $v.Type) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}