{{- /* generate part included in Hierarchy */ -}}
{{- define "metac_reflect_gen.hierarchy" -}}
{{-   if not (eq 0 (len .)) }}
.hierarchy_items_count = {{ len . }},
.hierarchy_items = (struct hierarchy_item[]){
{{-     range $_,$item := . }}
    {
        .link = &{{ $item.Id }},
{{-       with $item.DeclFile }}
        .p_decl_file = (metac_size_t[]){ {{ . }} },
{{-       end }}
{{-       with $item.DeclLine }}
        .p_decl_line = (metac_size_t[]){ {{ . }} },
{{-       end }}
{{-       with $item.DeclColumn }}
        .p_decl_column = (metac_size_t[]){ {{ . }} },
{{-       end }}
    },
{{-     end }}
},
{{-   end }}
{{- end -}}