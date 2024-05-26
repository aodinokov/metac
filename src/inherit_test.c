#include "metac/test.h"
#include "metac/inherit.h"

#define struct_base_as_name(_name_...) \
struct _name_{ \
    int a; \
    int b; \
}
METAC_STRUCT_CREATE(base);

#define struct_middle_as_name(_name_...) \
struct _name_{ \
    METAC_STRUCT_INHERIT0(base); \
    int x; \
}
METAC_STRUCT_CREATE(middle);

#define struct_top_as_name(_name_...) \
struct _name_ { \
    METAC_STRUCT_INHERIT1(middle); \
    int y; \
}
METAC_STRUCT_CREATE(top);

METAC_START_TEST(test_last_word_inx) {
    struct top x;
    x.base.a = 1;
    struct middle y;
    
    fail_unless(
        &x.base.a == &x.a && &x.base.b == &x.b &&
        &x.middle.a == &x.a && &x.middle.b == &x.b &&
        &x.middle.base.a == &x.a && &x.middle.base.b == &x.b);
}END_TEST