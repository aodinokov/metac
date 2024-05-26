{{- /* generate all */ -}}
{{- define "metac_test_gen" -}}
{{-   $path := index . 0 -}}
{{-   $dwarf := index . 1 -}}
{{-   $metaDb := dwarfToMetaDb $dwarf -}}
{{-   $resDict := dict -}}
{{-   include "metac_test_gen.build_requests" (dict "metaDb" $metaDb "resDict" $resDict) }}
#include <check.h>  /* all */
#include "metac/reqresp.h"

/* early declarations of requests */
{{-   if hasKey $resDict "test" -}}
{{-     range $_,$testName := get $resDict "test" }}
extern const TTest * METAC_REQUEST(test, {{ $testName }});
{{-     end -}}
{{-   end }}

int main(int argc, char **argv) {
    Suite *s = suite_create(argc>0?argv[0]:(__FILE__));
    TCase *tc = tcase_create("default");

    /* run tests  */
{{-   if hasKey $resDict "test" -}}
{{-     range $_,$testName := get $resDict "test" }}
    tcase_add_test(tc, METAC_REQUEST(test, {{ $testName }}));
{{-     end -}}
{{-   end }}

    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_FORK_GETENV);
    srunner_run_all(sr, CK_ENV);

    int f = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (f == 0) ? 0 : 1;
}
{{- end -}}