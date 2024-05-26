{{- define "metac_test_gen.build_requests" -}}
{{-   $metaDb := index . "metaDb" -}}
{{-   $resDict := index . "resDict" -}}
{{-   $prefix := "metac__" -}}
{{-   range $cu,$u := $metaDb.CompileUnits -}}
{{-     range $i,$h := $u.Hierarchy -}}
{{-       if eq $h.Kind "Variable" -}}
{{-         $var := index $metaDb.Variables $h.Id -}}
{{-         with $var.Name -}}
{{-           if hasPrefix $prefix $var.Name -}}
{{-             $splitted_request := regexSplit "_" (trimPrefix $prefix $var.Name) 2 }}
{{-             if eq 2 (len $splitted_request) -}}
{{-               $reqType := (index $splitted_request 0) -}}
{{-               $reqName := (index $splitted_request 1) -}}
{{-               if eq $reqType "test" -}}
{{-                 if hasKey $resDict $reqType -}}
{{-                   $_ := set $resDict $reqType (append (get $resDict $reqType) $reqName) -}}
{{-                 else -}}
{{-                   $_ := set $resDict $reqType (list $reqName) -}}
{{-                 end -}}
{{-               end -}}
{{-             end -}}
{{-           end -}}
{{-         end -}}
{{-       end -}}
{{-     end -}}
{{-   end -}}
{{- end -}}