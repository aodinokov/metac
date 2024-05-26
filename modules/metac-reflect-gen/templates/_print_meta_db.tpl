{{- define "metac_reflect_gen.print_meta_db" -}}
{{-   $dwarf := index . 0 -}}
{{-   toPrettyJson (dwarfToMetaDb $dwarf) -}}
{{- end -}}