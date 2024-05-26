{{- define "purelib.show_pretty_json" -}}
    json: {{ toPrettyJson . }}
{{- end -}}
