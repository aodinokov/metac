{{- /*small helpers to genearate little typical parts of objects (mixins in metadb) */ -}}
{{- define "metac_reflect_gen.name_mixin" -}}
{{-   with .Name }}
.name = "{{ . }}",
{{-   end }}
{{- end -}}

{{- define "metac_reflect_gen.declaration_mixin" -}}
{{-   with .Declaration -}}
{{-     if eq (toJson .) "true"  }}
.declaration = 1,
{{-     end -}}
{{-   end }}
{{- end -}}

{{- define "metac_reflect_gen.byte_size_mixin" -}}
{{-   with .ByteSize }}
.byte_size = {{ . }},
{{-   end -}}
{{- end -}}

{{- define "metac_reflect_gen.alignment_mixin" -}}
{{-   with .Alignment }}
.p_alignment = (metac_size_t[]){ {{ . }},},
{{-   end -}}
{{- end -}}

{{- define "metac_reflect_gen.obj_mixin" -}}
{{- $gen := false -}}
{{-   with .External -}}
{{-     $gen = true -}}
{{-   end -}}
{{-   with .LinkageName }}
{{-     $gen = true -}}
{{-   end -}}
{{    if $gen }}
.linkage_info = {
{{-     with .External -}}
{{-       if eq (toJson .) "true"  }}
    .external = 1,
{{-       end -}}
{{-     end -}}
{{-     with .LinkageName }}
    .linkage_name = "{{ . }}",
{{-     end }}
},
{{-   end -}}
{{- end -}}

{{- define "metac_reflect_gen.type" -}}
{{-   if and (ne .Id "") (ne .Id "void") }}
.type = &{{ .Id }},
{{-   end }}
{{- end -}}