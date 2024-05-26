{{- /* generate type parents - can be list because metadb has the deduplicated */ -}}
{{- define "metac_reflect_gen.type_parents" }}
.parents_count = {{ len .Parents }},
.parents = (metac_entry_t*[]){
{{-   range $_,$p := .Parents }}
    &{{ $p.Id }},
{{-   end }}
},
{{- end -}}
{{- /* generate parents for other items - always only 1 */ -}}
{{- define "metac_reflect_gen.parent" }}
.parents_count = 1,
.parents = (metac_entry_t*[]){
    &{{ .Parent.Id }},
},
{{- end -}}
