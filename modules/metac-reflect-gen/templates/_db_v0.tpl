{{- /* generate DB v0 */ -}}
{{- define "metac_reflect_gen.db_v0" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $resDict := index . "resDict" -}}
{{-   if hasKey $resDict "db" }}
{{-     $requests := get $resDict "db" -}}
{{-     $dbName := get (index $requests 0) "reqName" -}}
{{-     range $_,$r := $requests -}}
{{-       if ne $dbName (get $r "reqName") -}}
{{-         fail "got different db requests - not sure how to make" -}}
{{-       end -}}
{{-     end }}
/* db */
struct metac_db METAC_DB_NAME() = {
    .version = 0,
    .db_v0 = {
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "base_types" $metaDb.BaseTypes)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "enums" $metaDb.EnumerationTypes)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "typedefs" $metaDb.Typedefs)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "structs" $metaDb.StructTypes)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "unions" $metaDb.UnionTypes)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "classes" $metaDb.ClassTypes)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "namespaces" $metaDb.Namespaces)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "variables" $metaDb.Variables)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "subprograms" $metaDb.Subprograms)) -}}
{{-   indent 8 (include "metac_reflect_gen.name_to_entries" (dict "compile_units" $metaDb.CompileUnits)) }}
    },
};
{{-   end -}}
{{- end -}}
{{- /* helper */ -}}
{{- define "metac_reflect_gen.name_to_entries" -}}
{{-   range $key,$map := . -}}
{{-     if not (eq 0 (len $map)) -}}
{{-       $dict := dict -}}
{{-       range $i,$v := $map -}}
{{-         $name := "\"\"" -}}
{{-         with $v.Name -}}
{{-           $name = toJson $v.Name -}}
{{-         end -}}
{{-         if hasKey $dict $name -}}
{{-           $_ := set $dict $name (append (get $dict $name) $i) -}}
{{-         else -}}
{{-           $_ := set $dict $name (list $i) -}}
{{-         end -}}
{{-       end }}
.{{$key}} = {
    .items_count = {{ len $dict }},
    .items = (struct name_to_entries_info[]) {
{{-       range $i,$v := $dict }}
        {
            .name = {{ $i }},
            .entries_count = {{ len $v }},
            .entries = (metac_entry_t*[]) {
{{-         range $_,$e := $v }}
                &{{ $e }},
{{-         end }}
            },
        },
{{-       end }}
    },
},
{{-     end -}}
{{-   end -}}
{{- end -}}
