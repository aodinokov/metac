<img src="doc/logo/metac-logo-noborder-4171x1956.png" alt="metac logo" style="height: 110px;"/>

[![GoBuildAndTest](https://github.com/aodinokov/metac/actions/workflows/buildAndTest.yaml/badge.svg)](https://github.com/aodinokov/metac/actions/workflows/buildAndTest.yaml) [![Go Report Card](https://goreportcard.com/badge/github.com/aodinokov/metac)](https://goreportcard.com/report/github.com/aodinokov/metac) [![Go Reference](https://pkg.go.dev/badge/github.com/aodinokov/metac)](https://pkg.go.dev/github.com/aodinokov/metac)

/* ``If you can't do it in
ANSI C, it isn't worth doing.''
-- Unknown */

A framework that extends C with **reflection** and some other related golang-like features.
Reflection implementation is based on [DWARF](https://en.wikipedia.org/wiki/DWARF) data - one of the most common debugging information formats used by debuggers like gdb or lldb. Metac/reflect offers a familiar API similar in many ways to [Golang/reflect](https://pkg.go.dev/reflect) package for introspecting code and manipulating data at runtime. 

**Features**:

* Reflection for all native C types.
* 'Deep' functionality for printing, copying, comparing, and freeing memory of complex data structures.
* Supported on **Ubuntu, macOS, Windows (msys2)** with **gcc** or **clang**.

[**Structure fields printing example**](/examples/c_app_simplest/):

```c
#include <stdio.h>  // printf
#include <stdlib.h> // free
#include <math.h>   // M_PI, M_E
#include "metac/reflect.h"

struct test {
    int y;
    char c;
    double pi;
    double e;
    short _uninitialized_field;
};

int main(){
    // we need to use this construction to wrap variable declaration
    // to get its type information
    WITH_METAC_DECLLOC(decl_location,
        struct test t = {
            .y = -10,
            .c = 'a',
            .pi = M_PI,
            .e = M_E,
        };
    )
    metac_value_t *p_val = METAC_VALUE_FROM_DECLLOC(decl_location, t);

    char * s;
    s = metac_entry_cdecl(metac_value_entry(p_val));
    // next will output "struct test t = "
    printf("%s = ", s);
    free(s);

    s = metac_value_string(p_val);
    // next will output "{.y = -10, .c = 'a', .pi = 3.141593, .e = 2.718282, ._uninitialized_field = 0,};\n"
    printf("%s;\n", s);
    free(s);

    metac_value_delete(p_val);

    return 0;
}
```

[**Function parameters printing example**](/doc/demo/step_06/main.c):

```c
#include "metac/reflect.h"
#include <stdlib.h> /*free*/

#define METAC_WRAP_FN_RES(_tag_map_, _fn_, _args_...) ({ \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, _tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        _fn_(_args_);\
})

int test_function1_with_args(int a, short b){
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int main() {
    metac_value_t * p_val = NULL;
    // next will call test_function1_with_args and will output "fn returned: 38"
    printf("fn returned: %i\n", METAC_WRAP_FN_RES(NULL, test_function1_with_args, 10, 22));
    if (p_val != NULL) {
        char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL);
        if (s != NULL) {
            // next will output "captured test_function1_with_args(10, 22)"
            printf("captured %s\n", s);
            free(s);
        }
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(p_val);
        metac_parameter_storage_delete(p_param_storage);
        metac_value_delete(p_val);
    }
    return 0;
}
```

To get more details please refer to the [How to](/doc/demo/README.md#how-to-demo) document.