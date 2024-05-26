{{- define "metac_reflect_gen.escape_path" -}}
{{ regexReplaceAll "\\\\" . "/" }}
{{- end -}}