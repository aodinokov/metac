{{- define "localfn.show_pretty_json" -}}
    json: {{ toPrettyJson . }}
{{- end -}}
