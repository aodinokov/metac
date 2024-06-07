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

[**Example**](/examples/c_app_simplest/):

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

To get more details please refer to the [How to](/doc/demo/README.md#how-to-demo) document.