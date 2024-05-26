{{- /* walk via all CUs and find all vars which start from metac__[db|gsym|entry] and 
add to the map their CU and Hierarchy ID... for entry we may add more info*/ -}}
{{- define "metac_reflect_gen.build_requests" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $resDict := index . "resDict" -}}
{{-   $moduleDict := index . "moduleDict" -}}
{{-   $tmpDict := dict -}}
{{-   range $cu,$h := $metaDb.CompileUnits -}}
{{-     include "metac_reflect_gen.build_requests_hierarchy" (dict "module" "" "metaDb" $metaDb "hierarchy" $h.Hierarchy "resDict" $tmpDict "complieUnit" $cu "path" (list) "local" false) -}}
{{-   end -}}
{{-   $_ := include "metac_reflect_gen.module_name" (dict "resDict" $tmpDict "moduleDict" $moduleDict) -}}
{{-   range $module,$_ := $moduleDict -}}
{{-     range $cu,$h := $metaDb.CompileUnits -}}
{{-       include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $h.Hierarchy "resDict" $resDict "complieUnit" $cu "path" (list) "local" false) -}}
{{-     end -}}
{{-   end -}}
{{- end -}}
{{- /* helper function that isn't needed outside -recursively do what is needed */ -}}
{{- define "metac_reflect_gen.build_requests_hierarchy" -}}
{{-   $module := index . "module" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $hierarchy := index . "hierarchy" -}}
{{-   $resDict := index . "resDict" -}}
{{-   $cu := index . "complieUnit" -}}
{{-   $path := index . "path" -}}
{{-   $local := index . "local" -}}
{{-   $prefix := "metac__" -}}
{{-   if ne $module "" -}}
{{-     $prefix = printf "metac__%s_" $module -}}
{{-   end }}
{{-   range $i,$h := $hierarchy -}}
{{-     if eq $h.Kind "Variable" -}}
{{-       $var := index $metaDb.Variables $h.Id -}}
{{-       with $var.Name -}}
{{-         if hasPrefix $prefix $var.Name -}}
{{-           $splitted_request := regexSplit "_" (trimPrefix $prefix $var.Name) 2 }}
{{-           if eq 2 (len $splitted_request) -}}
{{-             $reqType := (index $splitted_request 0) -}}
{{-             $reqName := (index $splitted_request 1) -}}
{{-             $reference := dict "reqName" $reqName -}}
{{-             $_ := set $reference "module" $module -}}
{{-             $_ := set $reference "cu" $cu -}}
{{-             $_ := set $reference "path" $path -}}
{{-             $_ := set $reference "local" $local -}}
{{-             $_ := set $reference "h" $h -}}
{{-             $_ := set $reference "var" $var -}}
{{-             if hasKey $resDict $reqType -}}
{{-               $_ := set $resDict $reqType (append (get $resDict $reqType) $reference) -}}
{{-             else -}}
{{-               $_ := set $resDict $reqType (list $reference) -}}
{{-             end -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{-     if or (eq $h.Kind "Subprogram") (eq $h.Kind "Namespace") (eq $h.Kind "LexBlock") (eq $h.Kind "StructType") (eq $h.Kind "ClassType") (eq $h.Kind "UnionType") -}}
{{-       $local = eq $h.Kind "Subprogram" -}}
{{-       $path := append $path $i -}}
{{-       if eq $h.Kind "Subprogram" -}}
{{-         $var := index $metaDb.Subprograms $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-       if eq $h.Kind "Namespace" -}}
{{-         $var := index $metaDb.Namespaces $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-       if eq $h.Kind "LexBlock" -}}
{{-         $var := index $metaDb.LexBlocks $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-       if eq $h.Kind "StructType" -}}
{{-         $var := index $metaDb.StructTypes $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-       if eq $h.Kind "ClassType" -}}
{{-         $var := index $metaDb.ClassTypes $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-       if eq $h.Kind "UnionType" -}}
{{-         $var := index $metaDb.UnionTypes $h.Id  -}}
{{-         $_ := include "metac_reflect_gen.build_requests_hierarchy" (dict "module" $module "metaDb" $metaDb "hierarchy" $var.Hierarchy "resDict" $resDict "complieUnit" $cu "path" $path "local" $local) -}}
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}

{{- define "metac_reflect_gen.module_name" -}}
{{-   $resDict := index . "resDict" -}}
{{-   $moduleDict := index . "moduleDict" -}}
{{-   $_ := set $moduleDict "dflt" "" -}}
{{-   if hasKey $resDict "module" -}}
{{-     $requests := get $resDict "module" -}}
{{-     range $_,$r := $requests -}}
{{-       $_ := set $moduleDict (get $r "reqName") "" -}}
{{-     end -}}
{{-   end -}}
{{- end -}}