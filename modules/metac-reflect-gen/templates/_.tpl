{{- /* generate all */ -}}
{{- define "metac_reflect_gen" -}}
{{-   $dwarf := index . 0 -}}
{{-   $metaDb := dwarfToMetaDb $dwarf -}}
/* This file is auto-generated. Don't modify it if you aren't really sure about the reason. */
#include "metac/backend/entry.h"
/*
  Building data based on the following request: {{ "" }}
{{-   $resDict := dict -}}
{{-   $moduleDict := dict -}}
{{-   include "metac_reflect_gen.build_requests" (dict "metaDb" $metaDb "resDict" $resDict "moduleDict" $moduleDict) -}}
{{-   toPrettyJson $resDict }}
*/
{{- /* the next 2 are static objects only. they should be optimized with -O3 option if not used */ -}}
{{-   include "metac_reflect_gen.early_declarations" $metaDb -}}
{{-   include "metac_reflect_gen.actual_data" $metaDb -}}
{{- /* the next are templates for generating response for METAC_<>_NAME */ -}}
{{-   include "metac_reflect_gen.db_v0" (dict "metaDb" $metaDb "resDict" $resDict) -}}
{{-   include "metac_reflect_gen.glob_responses" (dict "metaDb" $metaDb "resDict" $resDict "key" "gsym" "resNameMacro" "METAC_GSYM_NAME") -}}
{{-   range $module,$_ := $moduleDict -}}
{{-     include "metac_reflect_gen.entry_db" (dict "module" $module "metaDb" $metaDb "resDict" $resDict) -}}
{{-   end -}}
{{- end -}}
