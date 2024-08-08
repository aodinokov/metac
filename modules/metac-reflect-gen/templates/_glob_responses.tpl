{{- /* generate GLOBVAR && SUBPROGRAM */ -}}
{{- define "metac_reflect_gen.glob_responses" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $resDict := index . "resDict" -}}
{{-   $key := index . "key" -}}
{{-   $resNameMacro := index . "resNameMacro" -}}
{{-   if hasKey $resDict $key -}}
{{-     $requests := get $resDict $key -}}
{{-     range $_,$r := $requests -}}
{{-       $cu := index $metaDb.CompileUnits (get $r "cu") -}}
{{-       $out := include "metac_reflect_gen.glob_responses_hierarchy" (dict "metaDb" $metaDb "hierarchy" $cu.Hierarchy "resNameMacro" $resNameMacro "resName" (get $r "reqName")) -}}
{{-       if eq $out "" -}}
{{- /* clang doesn't create declarations, so need to try all cu*/ -}}
{{-         range $_,$cu := $metaDb.CompileUnits -}}
{{-           if eq $out "" -}}
{{-             $out = include "metac_reflect_gen.glob_responses_hierarchy" (dict "metaDb" $metaDb "hierarchy" $cu.Hierarchy "resNameMacro" $resNameMacro "resName" (get $r "reqName")) -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-       $out -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
{{- /* helper to walk via hierarchy */ -}}
{{- define "metac_reflect_gen.glob_responses_hierarchy" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $hierarchy := index . "hierarchy" -}}
{{-   $resNameMacro := index . "resNameMacro" -}}
{{-   $resName := index . "resName" -}}
{{-   range $i,$h := $hierarchy -}}
{{-     if or (eq $h.Kind "Variable") (eq $h.Kind "Subprogram") -}}
{{-       $var := "" -}}
{{-       if eq $h.Kind "Variable" -}}
{{-         $var = index $metaDb.Variables $h.Id -}}
{{-       end -}}
{{-       if eq $h.Kind "Subprogram" -}}
{{-         $var = index $metaDb.Subprograms $h.Id -}}
{{-       end -}}
{{-       with $var -}}
{{-         if eq (toJson $resName) (toJson .Name) }}
struct metac_entry *{{ $resNameMacro }}({{ $var.Name }}) = &{{$h.Id}};
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{- /* TODO: C++ not sure about that - maybe we need only to do this for hierarchy siblings */ -}}
{{-     if or (eq $h.Kind "Namespace") -}}
{{-       if eq $h.Kind "Namespace" -}}
{{-         $var := index $metaDb.Namespaces $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "metaDb" $metaDb "hierarchy" $var.Hierarchy "resName" $resName "path" (list)) -}}
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
