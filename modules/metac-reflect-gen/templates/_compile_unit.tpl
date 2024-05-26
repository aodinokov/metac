{{- /* generate namespace declarations */ -}}
{{- define "metac_reflect_gen.compile_unit" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) }}
/* {{ $key }} */
{{-        range $i,$v := $map }}
static struct metac_entry {{ $i }} = {
    .kind = METAC_KND_compile_unit,
    .name = "{{ $v.Name }}",
    .compile_unit_info = {
        .lang = METAC_LANG_{{ $v.Lang }},
{{-     if not (eq 0 (len $v.DeclFiles)) }}
        .decl_files_count = {{ len $v.DeclFiles}}, 
        .decl_files = (metac_name_t[]){
{{-       range $_,$df := $v.DeclFiles }}
            "{{ include "metac_reflect_gen.escape_path" $df }}",
{{-       end }}
        },
{{-     end -}}
{{-     indent 8 (include "metac_reflect_gen.import" (tuple "imported_modules" $v.ImportedModules)) }}
{{-     indent 8 (include "metac_reflect_gen.import" (tuple "imported_declarations" $v.ImportedDeclarations)) }}
{{-     indent 8 (include "metac_reflect_gen.hierarchy" $v.Hierarchy) }}
    },
};
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}