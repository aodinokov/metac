{{- /* generate lex_block declarations */ -}}
{{- define "metac_reflect_gen.lex_block" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_lex_block,
{{-          indent 4 (include "metac_reflect_gen.parent" $v) }}
    .lex_block_info = {
{{-          indent 8 (include "metac_reflect_gen.hierarchy" $v.Hierarchy) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}