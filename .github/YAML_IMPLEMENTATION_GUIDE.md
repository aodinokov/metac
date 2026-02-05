# YAML Serialization Implementation Guide

## Quick Reference: API Implementation Checklist

This guide shows how to implement the YAML serialization API described in [.github/YAML_SERIALIZATION_DESIGN.md](.github/YAML_SERIALIZATION_DESIGN.md).

### File Structure

```
include/metac/serialization/yaml.h
  ├── metac_yaml_serialization_t struct definition
  ├── metac_yaml_serialization_new() declaration
  ├── metac_yaml_serialization_measure() declaration
  ├── metac_yaml_serialization_to_buffer() declaration
  └── metac_yaml_serialization_delete() declaration

src/serialization/yaml/
  ├── yaml_serialization.c
  │   ├── metac_yaml_serialization_new() - Create context
  │   ├── metac_yaml_serialization_measure() - Measure with caching
  │   ├── metac_yaml_serialization_to_buffer() - Serialize using cache
  │   └── metac_yaml_serialization_delete() - Clean up
  ├── yaml_handler.c
  │   ├── metac_yaml_write_handler() - Custom libyaml callback
  │   └── metac_yaml_emitter_init() - Setup emitter
  ├── yaml_events.c
  │   ├── metac_value_to_yaml_events() - Event generation loop
  │   └── emit_event_for_kind() - Per-type event generation
  └── yaml_test.c - Tests
```

---

## 1. Header: `include/metac/serialization/yaml.h`

```c
#ifndef INCLUDE_METAC_SERIALIZATION_YAML_H_
#define INCLUDE_METAC_SERIALIZATION_YAML_H_

#include "metac/reflect.h"
#include <yaml.h>

/**
 * @defgroup metac_yaml_serialization Cached YAML serialization (zero-malloc)
 * @{
 */

/**
 * @brief Opaque handle to reusable YAML serialization context
 * 
 * Stores cached iterator state to avoid redundant traversals.
 * User creates with metac_yaml_serialization_new(), measures with measure(),
 * then can serialize to multiple buffers with to_buffer().
 * 
 * Memory owned by metac - deleted via metac_yaml_serialization_delete().
 */
typedef struct metac_yaml_serialization metac_yaml_serialization_t;

/**
 * @brief Create reusable serialization context
 * 
 * Allocates context but does NOT traverse value yet.
 * First call to measure() or to_buffer() triggers structure analysis.
 * 
 * @param p_val       Value to serialize (user-owned, must outlive context)
 * @param wmode       Walk mode: METAC_WMODE_deep or METAC_WMODE_shallow
 * @param p_tag_map   Tag map for type resolution (NULL for defaults)
 * 
 * @return Allocated context, or NULL on allocation failure
 * 
 * @example
 *   struct data d = {...};
 *   WITH_METAC_DECLLOC(loc, struct data d = {...};)
 *   metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, d);
 *   
 *   metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
 *       pv, METAC_WMODE_deep, NULL);
 *   if (!serial) return -1;
 *   
 *   // ... use with measure() and to_buffer() ...
 *   
 *   metac_yaml_serialization_delete(serial);
 */
metac_yaml_serialization_t* metac_yaml_serialization_new(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map);

/**
 * @brief Measure required buffer size with iterator caching
 * 
 * Traverses value structure once, generates YAML events to 1-byte dummy buffer,
 * and CACHES the iterator state for reuse by subsequent to_buffer() calls.
 * 
 * This is the key optimization: instead of discarding traverse data like
 * the old metac_value_yaml_required_size(), we SAVE the iterator for reuse.
 * 
 * Can be called multiple times (recalculates if needed).
 * 
 * @param p_serial        Context from metac_yaml_serialization_new()
 * @param p_required_size (output, required) Bytes needed for full YAML
 * 
 * @return 0 on success
 * @return -EINVAL if p_serial or p_required_size NULL
 * @return -ENOMEM if cannot allocate iterator
 * @return -EIO if libyaml error during measurement
 * @return other errno on failure
 * 
 * @example
 *   size_t required;
 *   int err = metac_yaml_serialization_measure(serial, &required);
 *   if (err < 0) {
 *       fprintf(stderr, "measure failed: %s\n", strerror(-err));
 *       return err;
 *   }
 *   printf("YAML needs %zu bytes\n", required);
 */
int metac_yaml_serialization_measure(
    metac_yaml_serialization_t *p_serial,
    size_t *p_required_size);

/**
 * @brief Serialize to user buffer using cached structure data
 * 
 * Uses iterator cached from measure() call - NO redundant traversal!
 * If measure() not called yet, will call it internally.
 * 
 * Can be called multiple times with different buffers to serialize
 * the same value to multiple destinations (file, network, etc.)
 * using the same cached structure analysis.
 * 
 * @param p_serial        Context from metac_yaml_serialization_new()
 * @param p_buffer        User-allocated output buffer (not modified on error)
 * @param buffer_size     Available bytes in p_buffer
 * @param p_bytes_written (output, optional) Actual bytes written
 * 
 * @return 0 on success
 * @return -EINVAL if p_serial or p_buffer NULL
 * @return -ENOSPC if buffer too small (allocate buffer size from measure())
 * @return -EIO if libyaml error
 * @return other errno on failure
 * 
 * @example
 *   // Typical workflow
 *   size_t required;
 *   metac_yaml_serialization_measure(serial, &required);
 *   
 *   char *buf = malloc(required);
 *   int err = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
 *   if (err == 0) {
 *       fwrite(buf, 1, required, stdout);
 *   }
 *   free(buf);
 */
int metac_yaml_serialization_to_buffer(
    metac_yaml_serialization_t *p_serial,
    char *p_buffer,
    size_t buffer_size,
    size_t *p_bytes_written);

/**
 * @brief Delete serialization context
 * 
 * Frees cached iterator and internal state.
 * The value (p_val) is NOT modified - it's user-owned.
 * 
 * @param p_serial Context to delete (if NULL, no-op)
 */
void metac_yaml_serialization_delete(metac_yaml_serialization_t *p_serial);

/**
 * @brief Serialize value to dynamically allocated YAML string (single pass)
 * 
 * Convenience function for cases where user is OK with malloc:
 * - Single traversal (no measure + serialize)
 * - Internal buffer allocation
 * - Simple API
 * 
 * @param p_val       Value to serialize
 * @param wmode       Walk mode
 * @param p_tag_map   Tag map (NULL for defaults)
 * @param p_out_size  (output) String size including null terminator
 * 
 * @return Allocated YAML string (call metac_free to release)
 * @return NULL on allocation or serialization failure
 * 
 * @example
 *   size_t size;
 *   char *yaml = metac_value_to_yaml_string(p_val, METAC_WMODE_deep, NULL, &size);
 *   if (yaml) {
 *       puts(yaml);
 *       metac_free(yaml);
 *   }
 */
char* metac_value_to_yaml_string(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    size_t *p_out_size);

/** @} */
#endif // INCLUDE_METAC_SERIALIZATION_YAML_H_
```

---

## 2. Context Initialization: `src/serialization/yaml/yaml_context.c`

```c
#include "metac/serialization/yaml.h"
#include "metac/backend/value.h"
#include <string.h>
#include <errno.h>

/* Internal context structure (opaque to user) */
typedef struct {
    yaml_emitter_t emitter;
    char *p_buffer;
    size_t buffer_size;
    size_t current_pos;
    int overflow;
} metac_yaml_emitter_state_t;

/**
 * Write handler called by libyaml when it has data to emit
 * 
 * libyaml calls this repeatedly with small chunks of YAML text.
 * We copy directly to user buffer. If buffer fills, signal failure.
 */
static int metac_yaml_write_handler(void *data, unsigned char *buffer,
                                    size_t size, void *ext) {
    metac_yaml_emitter_state_t *state = (metac_yaml_emitter_state_t *)ext;
    
    if (!state || !buffer) {
        return 0;
    }
    
    /* Check overflow BEFORE writing */
    if (state->current_pos + size > state->buffer_size) {
        state->overflow = 1;
        return 0;  /* Fail - signals to libyaml to stop emitting */
    }
    
    /* Direct copy: zero intermediate buffer */
    memcpy(state->p_buffer + state->current_pos, buffer, size);
    state->current_pos += size;
    
    return 1;  /* Success */
}

/**
 * Setup emitter with custom output handler
 */
static int metac_yaml_emitter_setup(metac_yaml_emitter_state_t *state,
                                    char *p_buffer,
                                    size_t buffer_size) {
    state->p_buffer = p_buffer;
    state->buffer_size = buffer_size;
    state->current_pos = 0;
    state->overflow = 0;
    
    if (!yaml_emitter_initialize(&state->emitter)) {
        return -EIO;
    }
    
    /* This is the KEY: register custom write callback */
    yaml_emitter_set_output(&state->emitter,
                           metac_yaml_write_handler,
                           state);
    
    /* Configuration */
    yaml_emitter_set_canonical(&state->emitter, 0);
    yaml_emitter_set_indent(&state->emitter, 2);
    yaml_emitter_set_width(&state->emitter, 80);
    
    return 0;
}

/**
 * Cleanup emitter
 */
static void metac_yaml_emitter_cleanup(metac_yaml_emitter_state_t *state) {
    if (state) {
        yaml_emitter_delete(&state->emitter);
    }
}
```

---

## 3. Event Generation Loop: `src/serialization/yaml/yaml_emitter.c`

This is the core: iterator → event logic.

```c
#include "metac/serialization/yaml.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"
#include <yaml.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
    metac_yaml_emitter_state_t emitter_state;
    metac_recursive_iterator_t *p_iterator;
    metac_value_walk_mode_t wmode;
    metac_tag_map_t *p_tag_map;
} metac_yaml_context_t;

/**
 * Helper: emit a scalar value
 */
static int emit_scalar(yaml_emitter_t *emitter, const char *value,
                       size_t length) {
    yaml_event_t event;
    
    if (!value) {
        value = "null";
        length = 4;
    }
    
    if (!yaml_scalar_event_initialize(&event, NULL, NULL,
                                     (yaml_char_t *)value, (int)length,
                                     1, 1, YAML_PLAIN_SCALAR_STYLE)) {
        return 0;
    }
    
    return yaml_emitter_emit(emitter, &event);
}

/**
 * Main serialization loop: iterator yields tasks, we emit YAML
 */
static int metac_value_to_yaml_impl(metac_value_t *p_root,
                                    metac_yaml_context_t *p_ctx) {
    yaml_event_t event;
    yaml_emitter_t *emitter = &p_ctx->emitter_state.emitter;
    
    /* Stream start */
    if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING) ||
        !yaml_emitter_emit(emitter, &event)) {
        return -EIO;
    }
    
    /* Document start */
    if (!yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0) ||
        !yaml_emitter_emit(emitter, &event)) {
        return -EIO;
    }
    
    /* Create iterator for traversal */
    p_ctx->p_iterator = metac_new_recursive_iterator(p_root);
    if (!p_ctx->p_iterator) {
        return -ENOMEM;
    }
    
    /* Main loop: process one task per iteration */
    for (void *p_task = metac_recursive_iterator_next(p_ctx->p_iterator);
         p_task != NULL;
         p_task = metac_recursive_iterator_next(p_ctx->p_iterator)) {
        
        metac_value_t *p_val = (metac_value_t *)p_task;
        metac_r_iter_state_t state = metac_recursive_iterator_get_state(p_ctx->p_iterator);
        
        if (state == METAC_R_ITER_start) {
            metac_kind_t kind = metac_value_final_kind(p_val, NULL);
            
            switch (kind) {
            
            case METAC_KND_struct_type: {
                /* Emit struct as YAML mapping */
                if (!yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
                                                        YAML_BLOCK_MAPPING_STYLE) ||
                    !yaml_emitter_emit(emitter, &event)) {
                    goto error;
                }
                
                metac_entry_t *p_entry = metac_value_entry(p_val);
                metac_num_t member_count = metac_entry_member_count(p_entry);
                
                /* Create task for each field */
                for (metac_num_t i = 0; i < member_count; ++i) {
                    metac_entry_t *p_member = metac_entry_member(p_entry, i);
                    if (!metac_recursive_iterator_create_and_append_dep(
                            p_ctx->p_iterator, p_member)) {
                        goto error;
                    }
                }
                
                metac_recursive_iterator_set_state(p_ctx->p_iterator, 1);
                break;
            }
            
            case METAC_KND_array_type: {
                /* Emit array as YAML sequence */
                if (!yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
                                                         YAML_BLOCK_SEQUENCE_STYLE) ||
                    !yaml_emitter_emit(emitter, &event)) {
                    goto error;
                }
                
                metac_entry_t *p_entry = metac_value_entry(p_val);
                metac_num_t length = 0;
                metac_entry_array_length_get(p_entry, &length);
                
                for (metac_num_t i = 0; i < length; ++i) {
                    metac_entry_t *p_elem_type = metac_entry_array_element_type(p_entry);
                    if (!metac_recursive_iterator_create_and_append_dep(
                            p_ctx->p_iterator, p_elem_type)) {
                        goto error;
                    }
                }
                
                metac_recursive_iterator_set_state(p_ctx->p_iterator, 1);
                break;
            }
            
            case METAC_KND_base_type:
            case METAC_KND_enum_type: {
                /* Scalar - generate string representation */
                char *p_str = metac_value_string(p_val);
                if (!p_str) {
                    if (!emit_scalar(emitter, "null", 4)) {
                        goto error;
                    }
                } else {
                    if (!emit_scalar(emitter, p_str, strlen(p_str))) {
                        free(p_str);
                        goto error;
                    }
                    free(p_str);
                }
                
                metac_recursive_iterator_done(p_ctx->p_iterator, NULL);
                break;
            }
            
            default:
                /* Unsupported kind */
                metac_recursive_iterator_fail(p_ctx->p_iterator);
            }
            
        } else if (state == 1) {
            /* Complete container (struct/array) */
            metac_kind_t kind = metac_value_final_kind(p_val, NULL);
            
            if (kind == METAC_KND_struct_type) {
                if (!yaml_mapping_end_event_initialize(&event) ||
                    !yaml_emitter_emit(emitter, &event)) {
                    goto error;
                }
            } else if (kind == METAC_KND_array_type) {
                if (!yaml_sequence_end_event_initialize(&event) ||
                    !yaml_emitter_emit(emitter, &event)) {
                    goto error;
                }
            }
            
            metac_recursive_iterator_done(p_ctx->p_iterator, NULL);
        }
        
        /* Check for buffer overflow */
        if (p_ctx->emitter_state.overflow) {
            return -ENOSPC;
        }
    }
    
    /* Document end */
    if (!yaml_document_end_event_initialize(&event, 0) ||
        !yaml_emitter_emit(emitter, &event)) {
        return -EIO;
    }
    
    /* Stream end */
    if (!yaml_stream_end_event_initialize(&event) ||
        !yaml_emitter_emit(emitter, &event)) {
        return -EIO;
    }
    
    return 0;
    
error:
    return -EIO;
}
```

---

## 4. Public API: `src/serialization/yaml/api.c`

```c
#include "metac/serialization/yaml.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int metac_value_to_yaml_buffer(metac_value_t *p_val,
                               char *p_buffer,
                               size_t buffer_size,
                               metac_value_walk_mode_t wmode,
                               metac_tag_map_t *p_tag_map,
                               size_t *p_bytes_written) {
    metac_yaml_context_t ctx = {0};
    int ret;
    
    if (!p_val || !p_buffer || buffer_size == 0) {
        return -EINVAL;
    }
    
    ctx.wmode = wmode;
    ctx.p_tag_map = p_tag_map;
    
    ret = metac_yaml_emitter_setup(&ctx.emitter_state, p_buffer, buffer_size);
    if (ret < 0) {
        return ret;
    }
    
    ret = metac_value_to_yaml_impl(p_val, &ctx);
    
    if (p_bytes_written) {
        *p_bytes_written = ctx.emitter_state.current_pos;
    }
    
    metac_yaml_emitter_cleanup(&ctx.emitter_state);
    
    if (ctx.p_iterator) {
        metac_recursive_iterator_free(ctx.p_iterator);
    }
    
    return ret;
}

int metac_value_yaml_required_size(metac_value_t *p_val,
                                   size_t *p_required_size,
                                   metac_value_walk_mode_t wmode,
                                   metac_tag_map_t *p_tag_map) {
    char dummy[1];
    size_t required;
    
    metac_value_to_yaml_buffer(p_val, dummy, 1, wmode, p_tag_map, &required);
    
    if (p_required_size) {
        *p_required_size = required;
    }
    
    return -ENOSPC;  /* Always indicates "buffer too small" but gives size */
}
```

---

## 4. Convenience Function (Shared Code Pattern)

### Architecture: Single Wrapper, Shared Core

**Key principle**: The convenience function uses **the exact same code** as the context API. It's not a separate implementation branch - it's a thin wrapper.

```
metac_value_to_yaml_string()
    ↓ (thin wrapper - 10 lines)
    ├─ metac_yaml_serialization_new() ← SAME CODE
    ├─ metac_yaml_serialization_measure() ← SAME CODE
    ├─ metac_yaml_serialization_to_buffer() ← SAME CODE
    └─ metac_yaml_serialization_delete() ← SAME CODE
```

**Benefits**:
- ✅ No code duplication
- ✅ Bug fixes apply to both APIs automatically
- ✅ Shared caching logic
- ✅ Easy to maintain
- ✅ Single source of truth for core algorithm

### Implementation: `src/serialization/yaml/api.c`

```c
#include "metac/serialization/yaml.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Serialize value to dynamically allocated YAML string
 * 
 * This is a THIN WRAPPER around the context API.
 * It reuses all core functions - just orchestrates them and handles malloc.
 * 
 * Implementation:
 *   1. metac_yaml_serialization_new()      ← Core function #1
 *   2. metac_yaml_serialization_measure()  ← Core function #2
 *   3. malloc() buffer
 *   4. metac_yaml_serialization_to_buffer() ← Core function #3
 *   5. metac_yaml_serialization_delete()   ← Core function #4
 * 
 * All heavy lifting is in the shared core functions.
 * This function is just ~15 lines of orchestration + error handling.
 */
char* metac_value_to_yaml_string(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    size_t *p_out_size) {
    
    if (!p_val || !p_out_size) {
        return NULL;
    }
    
    /* Step 1: Create context using SHARED core function */
    metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
        p_val, wmode, p_tag_map);
    if (!serial) {
        *p_out_size = 0;
        return NULL;
    }
    
    /* Step 2: Measure using SHARED core function */
    size_t required;
    int err = metac_yaml_serialization_measure(serial, &required);
    if (err < 0) {
        metac_yaml_serialization_delete(serial);
        *p_out_size = 0;
        return NULL;
    }
    
    /* Step 3: Allocate buffer (this is the ONLY malloc-specific code) */
    char *buf = malloc(required + 1);  /* +1 for null terminator */
    if (!buf) {
        metac_yaml_serialization_delete(serial);
        *p_out_size = 0;
        return NULL;
    }
    
    /* Step 4: Serialize using SHARED core function */
    size_t written;
    err = metac_yaml_serialization_to_buffer(serial, buf, required, &written);
    if (err < 0) {
        free(buf);
        metac_yaml_serialization_delete(serial);
        *p_out_size = 0;
        return NULL;
    }
    
    /* Step 5: Cleanup using SHARED core function */
    metac_yaml_serialization_delete(serial);
    
    /* Final touch: null-terminate and return */
    buf[written] = '\0';
    *p_out_size = written + 1;  /* Include null terminator */
    return buf;
}

/**
 * @brief Free YAML string from metac_value_to_yaml_string()
 * 
 * Simple wrapper for clarity. Callers can do:
 *   char *yaml = metac_value_to_yaml_string(...);
 *   metac_yaml_free(yaml);  // Semantic: "freed by metac"
 * 
 * Instead of:
 *   free(yaml);  // Less clear where allocation came from
 */
void metac_yaml_free(char *p_str) {
    if (p_str) {
        free(p_str);
    }
}
```

### Code Sharing Analysis

**Lines of code**:
- `metac_value_to_yaml_string()`: ~25 lines (mostly error handling)
- `metac_yaml_free()`: 3 lines
- **Total new code**: ~28 lines

**Shared code reused**:
- `metac_yaml_serialization_new()`: ~50 lines (shared)
- `metac_yaml_serialization_measure()`: ~80 lines (shared)
- `metac_yaml_serialization_to_buffer()`: ~100 lines (shared)
- `metac_yaml_serialization_delete()`: ~20 lines (shared)
- **Total shared**: ~250 lines

**Ratio**: ~28 new lines of wrapper code : ~250 lines of shared core = **90% code reuse**

### Maintenance: Why This Matters

When you need to fix a bug in YAML serialization:

```
Bug: "YAML output corrupts nested arrays"

FIX: Patch metac_yaml_serialization_to_buffer()

RESULT:
  ✅ Context API users: fixed
  ✅ Single-pass API users: fixed (uses same function!)
  ✅ No need to fix two separate implementations
```

**Example**: If you optimize the iterator caching:
```c
// One change in metac_yaml_serialization_measure():
// "Use faster cache lookup strategy"

// Automatically benefits:
// - Direct context API calls
// - Single-pass wrapper calls
// - No duplicate fixes needed
```

### Build Integration

**File structure**:
```
src/serialization/yaml/
├── yaml_context.c       ← Core context functions
├── yaml_emitter.c       ← Core event loop
├── yaml_handler.c       ← libyaml write handler
├── api.c               ← ONLY convenience wrapper
└── yaml_test.c         ← Tests for both APIs
```

**Compilation** (single rule):
```makefile
# All sources compile together
YAML_SRCS = src/serialization/yaml/yaml_context.c \
            src/serialization/yaml/yaml_emitter.c \
            src/serialization/yaml/yaml_handler.c \
            src/serialization/yaml/api.c

libmetac_reflect += $(YAML_SRCS)
```

No separate build targets needed - convenience function compiles with the rest.

### Comparison: Architecture Patterns

| Pattern | Code | Maintenance | Performance |
|---------|------|-------------|-------------|
| **Wrapper approach** (what we do) | ~28 lines wrapper + 250 shared | Easy ✅ | Same as context API |
| **Duplicate code** (bad) | 28 lines wrapper + 250 duplicated | Hard ❌ | Same but risky |
| **Macro wrapper** | ~5 lines macro | Hard ❌ | Potential bugs |
| **Two implementations** | 28 + 250 + 250 | Very hard ❌ | Inconsistent |

Our choice: **Wrapper approach** - minimal code, maximum reuse, easy maintenance.

---

## 5. Integration: Build Rules

Add to [Makefile](../Makefile):

```makefile
# YAML serialization backend
libmetac_reflect+= \
    src/serialization/yaml/yaml_context.c \
    src/serialization/yaml/yaml_emitter.c \
    src/serialization/yaml/api.c

# Link libyaml (pkg-config)
LDFLAGS += $(shell pkg-config --libs yaml-0.1)
CFLAGS += $(shell pkg-config --cflags yaml-0.1)
```

---

## 6. Testing: `src/serialization/yaml/yaml_test.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include "metac/reflect.h"
#include "metac/serialization/yaml.h"

/* Test 1: Context API (measure + buffer) */
START_TEST(test_yaml_context_api_simple_struct) {
    struct point {
        int x;
        int y;
    };
    
    WITH_METAC_DECLLOC(loc,
        struct point p = {.x = 10, .y = 20};
    )
    
    metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, p);
    ck_assert_ptr_nonnull(pv);
    
    /* Create context */
    metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
        pv, METAC_WMODE_deep, NULL);
    ck_assert_ptr_nonnull(serial);
    
    /* Measure */
    size_t required;
    int err = metac_yaml_serialization_measure(serial, &required);
    ck_assert_int_eq(err, 0);
    ck_assert_int_gt(required, 0);
    
    /* Allocate and serialize */
    char *buffer = malloc(required);
    size_t written;
    err = metac_yaml_serialization_to_buffer(serial, buffer, required, &written);
    ck_assert_int_eq(err, 0);
    ck_assert_int_eq(written, required);
    
    /* Verify output */
    ck_assert(strstr(buffer, "x:") != NULL);
    ck_assert(strstr(buffer, "y:") != NULL);
    
    free(buffer);
    metac_yaml_serialization_delete(serial);
    metac_value_delete(pv);
}
END_TEST

/* Test 2: Single-pass convenience API */
START_TEST(test_yaml_single_pass_convenience) {
    struct person {
        char name[32];
        int age;
    };
    
    WITH_METAC_DECLLOC(loc,
        struct person john = {.name = "John", .age = 30};
    )
    
    metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, john);
    ck_assert_ptr_nonnull(pv);
    
    /* Single-pass API - everything in one call */
    size_t size;
    char *yaml = metac_value_to_yaml_string(
        pv, METAC_WMODE_deep, NULL, &size);
    
    ck_assert_ptr_nonnull(yaml);
    ck_assert_int_gt(size, 0);
    ck_assert_int_eq(yaml[size - 1], '\0');  /* Null-terminated */
    
    /* Verify output */
    ck_assert(strstr(yaml, "name:") != NULL);
    ck_assert(strstr(yaml, "age:") != NULL);
    
    metac_yaml_free(yaml);
    metac_value_delete(pv);
}
END_TEST

/* Test 3: Context reuse (multiple serializations) */
START_TEST(test_yaml_context_reuse) {
    struct data {
        int value;
    };
    
    WITH_METAC_DECLLOC(loc,
        struct data d = {.value = 42};
    )
    
    metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, d);
    
    metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
        pv, METAC_WMODE_deep, NULL);
    
    size_t required;
    metac_yaml_serialization_measure(serial, &required);
    
    char *buf = malloc(required);
    
    /* First serialization */
    int err1 = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
    ck_assert_int_eq(err1, 0);
    char *yaml1 = strdup(buf);  /* Save first output */
    
    /* Second serialization (reuses cached data!) */
    int err2 = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
    ck_assert_int_eq(err2, 0);
    
    /* Both should be identical */
    ck_assert_str_eq(yaml1, buf);
    
    free(yaml1);
    free(buf);
    metac_yaml_serialization_delete(serial);
    metac_value_delete(pv);
}
END_TEST
```

---

## Key Takeaways

1. **Custom Write Handler**: `yaml_emitter_set_output()` + callback = zero malloc in metac
2. **Caching Strategy**: measure() caches iterator; to_buffer() reuses it (single traversal per measure)
3. **Two APIs**: Context API for control + reusability; single-pass for simplicity
4. **Buffer Control**: User provides buffer; write handler tracks position and overflow
5. **Error Handling**: Return -ENOSPC on buffer exhaustion with size info
6. **Memory Safety**: Iterator frees all tasks; no leaks on error paths
