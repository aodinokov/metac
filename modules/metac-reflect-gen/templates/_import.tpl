{{- /* generate part included in importes, e.g. imported_modules */ -}}
{{- define "metac_reflect_gen.import" -}}
{{-   $as := index . 0 -}}
{{-   $imports := index . 1 -}}
{{-   if not (eq 0 (len $imports)) }}
.{{ $as }}_count = {{ len $imports }}
.{{ $as }} = (metac_entry_t*[]){
{{-     range $_,$item := $imports}}
    &{{ $item.Id }},
{{-     end }}
},
{{-   end }}
{{- end -}}