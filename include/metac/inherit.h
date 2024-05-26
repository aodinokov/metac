#ifndef INCLUDE_METAC_INHERIT_H_
#define INCLUDE_METAC_INHERIT_H_

/** \page inheritance_emulation Inheritance emulation
 
This is more like the Golang encapsulation effect. This allows
all fields or struct to be accessible without `parent_field.` prefix and with this prefix. 

Basically we can get the effect of Golang:
```
type Common struct {
	someCommonData string
}

type Child1 struct {
	Common

	someSpecificData string
}
```

If we do this in Golang the, we can access `someCommonData` either directly like `var.someCommonData` in the object Child1 
or via `var.Common.someCommonData`.

This works only for C11 and further, because it needs anon unions and struct in other struct.

For doing so in C it's necessary to define Common struct in Macro:

```c
#define struct_common_as_name(_name_...) \
struct _name_{ \
    char * some_common_data; \
}
METAC_STRUCT_CREATE(common);

#define struct_child1_as_name(_name_...) \
struct _name_{ \
    METAC_STRUCT_INHERIT0(common); \
    char * some_specific_data; \
}
METAC_STRUCT_CREATE(child1);
```

Now if you use struct child1 `some_common_data` can be accessible as:

```c
int some_func() {
    child1 var;

    var.some_common_data = "data";
    printf("%s\n", var.common.some_common_data); // output will be 'data\n'

    return 0;
}
```

This is convenient to build some objects that still can be operable with functions of their encapsulated objects,
but you don't need to write very long expressions to access fields, so the object would look like one big object for the user.


This module allows 7-layers for now, but it can be extended just by adding more `METAC_STRUCT_INHERIT...(_name_)` macroses.
each level must use its own macro, they can't be reused on the next layer.
*/

#define METAC_STRUCT_CREATE(_name_) struct_## _name_ ##_as_name(_name_)
#define METAC_STRUCT_INHERIT0(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT1(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT2(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT3(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT4(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT5(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
#define METAC_STRUCT_INHERIT6(_name_)  \
    union { \
        struct_## _name_ ##_as_name(); \
        struct _name_ _name_; \
    }
/* all the same, but we can't use the same macro twice in the same hierarchy ...can be repeated if needed more */

#endif