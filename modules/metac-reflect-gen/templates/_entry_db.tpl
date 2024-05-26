{{- /* generate entries DB with var locations */ -}}
{{- define "metac_reflect_gen.entry_db" -}}
{{-   $module := index . "module" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $resDict := index . "resDict" -}}
{{-   if hasKey $resDict "entry" -}}
{{-     $requests := get $resDict "entry" -}}
{{-     $reqLocDict := dict -}}
{{-     $reqLocAndNameDict := dict -}}
{{-     $edb := dict -}}
{{-     range $_,$r := $requests -}}
{{-       if eq (get $r "module") $module -}}
{{-         $h := (get $r "h") -}}
{{-         $file := "" -}}
{{-         $line := -1 -}}
{{-         $cu := index $metaDb.CompileUnits (get $r "cu") -}}
{{-         with $h.DeclFile -}}
{{-           $file = index $cu.DeclFiles (atoi (toJson .)) -}}
{{-         end -}}
{{-         with $h.DeclLine -}}
{{-           $line = (atoi (toJson .)) -}}
{{-         end -}}
{{- /* we don't support several locdecl with the same name on the same line - FAIL in that case */ -}}
{{-         $reqLocAndName := printf "%s:%d %s" $file $line (get $r "reqName") -}}
{{-         if hasKey $reqLocAndNameDict $reqLocAndName -}}
{{-            fail (printf "location: %s name repeats at least twice in the same place" $reqLocAndName) -}}
{{-         end -}}
{{-         $_ := set $reqLocAndNameDict $reqLocAndName (get $r "reqName") -}}
{{- /* dicts are sorted in alpha order */ -}}
{{-         $sloc_req_var := (get $r "var") -}}
{{-         $sloc := toJson $sloc_req_var.Name -}}
{{-         $vars := dict -}}
{{- /* skip location if we already called that for another declloc */ -}}
{{-         $reqLoc := printf "%s:%d" $file $line -}}
{{-         if true -}}
{{-           $_ := set $reqLocDict $reqLoc "" -}}
{{- /* now need to scan $cu and to put all vars with the same $file:$line to vars */ -}}
{{-           $_ := include "metac_reflect_gen.recur_add_all_siblings" (dict "metaDb" $metaDb "curObj" $sloc_req_var "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-           range $varEntry,$varName := $vars -}}
{{- /* {{$sloc}} => {{$varEntry}} {{$varName}}*/ -}}
{{-             if hasKey $edb $file -}}
{{-                $edbf := get $edb $file -}}
{{-                if hasKey $edbf $sloc -}}
{{-                  $edbfl := get $edbf $sloc -}}
{{-                  if not (hasKey $edbfl $varName) -}}
{{-                    $_ := set $edbfl $varName (list $varEntry) -}}
{{-                  else -}}
{{-                    $_ := set $edbfl $varName (append (get $edbfl $varName) $varEntry) -}}
{{-                  end -}}
{{-                else -}}
{{-                  $_ := set $edbf $sloc (dict $varName (list $varEntry)) -}}
{{-                end -}}
{{-             else -}}
{{-               $_ := set $edb $file (dict $sloc (dict $varName (list $varEntry))) -}}
{{-             end -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{-     if ne (len $edb) 0 }}
struct metac_entry_db *METAC_ENTRY_DB_NAME({{$module}}) = (struct metac_entry_db[]) {
    {
        .per_file_item_count = {{len $edb}},
        .per_file_items = (struct metac_per_file_item[]) {
{{-       range $file,$dFile := $edb }}
            {
                .file = "{{ include "metac_reflect_gen.escape_path" $file}}",
                .per_loc_item_count = {{len $dFile}},
                .per_loc_items = (struct metac_per_loc_item[]) {
{{-         range $loc,$dLoc := $dFile }}
                    {
                        .loc_name = {{$loc}},
                        .per_name_item_count = {{len $dLoc}},
                        .per_name_items = (struct metac_per_name_item[]) {
{{-           range $name,$entryArr := $dLoc }}
                            {
                                .name = {{$name}},
                                .entry_count = {{len $entryArr}}, 
                                .pp_entries = (metac_entry_t *[]) {
{{-             range $_,$entry := $entryArr }}
                                    &{{$entry}},
{{-             end }}
                                },
                            },
{{-           end }}
                        },
                    },
{{-         end }}
                },
            },
{{-       end }}
        },
    },
};
{{-     end -}}
{{-   end -}}
{{- end -}}
{{- /* approach with look for visibility zones:
    looks first for siblins in the same parent and store all into dict if the line is the same.
    next - go to the siblins of parent, but if the name exists - don't store it.. and so on.
    that could be enough.
    This means - we have a new requirement - DECLLOC AND object must be in the same block ideally
 */ -}}
{{- define "metac_reflect_gen.recur_add_all_siblings" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $cu := index . "cu" -}}
{{-   $curObj := index . "curObj" -}}
{{-   $file := index . "file" -}}
{{-   $line := index . "line" -}}
{{-   $vars := index . "vars" }}
/* siblings for {{ . }} */
{{-   with $curObj.Parent }}
/* entered in {{ . }} */
{{-     $parent := . -}}
{{-     if eq $parent.Kind "Subprogram" -}}
{{-       $var := index $metaDb.Subprograms $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "Namespace" -}}
{{-       $var := index $metaDb.Namespaces $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "LexBlock" -}}
{{-       $var := index $metaDb.LexBlocks $parent.Id  -}}
{{-       include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "StructType" -}}
{{-       $var := index $metaDb.StructTypes $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "ClassType" -}}
{{-       $var := index $metaDb.ClassTypes $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "UnionType" -}}
{{-       $var := index $metaDb.UnionTypes $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-     if eq $parent.Kind "CompileUnit" -}}
{{-       $var := index $metaDb.CompileUnits $parent.Id  -}}
{{-       $_ := include "metac_reflect_gen.add_all_siblings" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "vars" $vars "cu" $cu "file" $file "line" $line) -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
{{- /* include all objects in hierarchy */ -}}
{{- define "metac_reflect_gen.add_all_siblings" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $hierarchy := index . "hierarchy" -}}
{{-   $cu := index . "cu" -}}
{{-   $file := index . "file" -}}
{{-   $line := index . "line" -}}
{{-   $vars := index . "vars" -}}
{{-   range $i,$h := $hierarchy -}}
{{-     if eq $h.Kind "Variable" -}}
{{-       $dvar := index $metaDb.Variables $h.Id -}}
{{-       $cfile := "" -}}
{{-       $cline := -1 -}}
{{-       with $h.DeclFile -}}
{{-         $cfile = index $cu.DeclFiles (atoi (toJson .)) -}}
{{-       end -}}
{{-       with $h.DeclLine -}}
{{-         $cline = (atoi (toJson .)) -}}
{{-       end -}}
{{-       if and (eq $file $cfile) (eq $line $cline) -}}
{{-         with $dvar.Name -}}
{{-           if not (hasKey $vars (toString $h.Id)) -}}
{{-             $_ := set $vars (toString $h.Id) (toJson .) -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{- /* check all recursivly */ -}}
{{-     if eq $h.Kind "Subprogram" -}}
{{-       $var := index $metaDb.Subprograms $h.Id  -}}
{{-       $cfile := "" -}}
{{-       $cline := -1 -}}
{{-       with $h.DeclFile -}}
{{-         $cfile = index $cu.DeclFiles (atoi (toJson .)) -}}
{{-       end -}}
{{-       with $h.DeclLine -}}
{{-         $cline = (atoi (toJson .)) -}}
{{-       end -}}
{{-       if and (eq $file $cfile) (eq $line $cline) -}}
{{-         with $var.Name -}}
{{-           if not (hasKey $vars (toString $h.Id)) -}}
{{-             $_ := set $vars (toString $h.Id) (toJson .) -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}