# YAML Serialization API Design for Metac

**Objective**: Design an event-driven YAML serialization API where metac acts as an event provider for `libyaml` emitter, with user-controlled memory allocation and **cached, reusable context** to avoid redundant traversals.

## 1. Core Context Structure: `metac_yaml_serialization_t`

The key design improvement: a reusable serialization context that caches intermediate data.

```c
#ifndef INCLUDE_METAC_YAML_SERIALIZATION_H_
#define INCLUDE_METAC_YAML_SERIALIZATION_H_

#include "metac/backend/value.h"

/**
 * @brief Opaque serialization context
 * 
 * Stores:
 *   - The value to serialize (reference only)
 *   - Walk mode and tag map settings
 *   - Cached iterator state after measure()
 *   - Structure information (field counts, etc.)
 * 
 * This allows:
 *   - Single measure() pass to cache structure
 *   - Multiple to_buffer() calls using cached data
 *   - No redundant iterator creation or traversal
 */
typedef struct metac_yaml_serialization {
    metac_value_t *p_val;              /**< Value to serialize (user-owned) */
    metac_value_walk_mode_t walk_mode; /**< Deep or shallow */
    metac_tag_map_t *p_tag_map;        /**< Type resolution */
    
    /* Cached after measure() */
    metac_recursive_iterator_t *p_iterator_cached; /**< Saved iterator state */
    size_t required_size;              /**< Measured size (0 = not measured) */
    int measured;                      /**< Flag: measure() called? */
    
    /* libyaml emitter (created fresh for each to_buffer() call) */
    yaml_emitter_t emitter;            /**< Emitter instance */
    
    /* Output tracking */
    char *p_buffer;
    size_t buffer_size;
    size_t current_pos;
    int overflow;
    
    /* Error state */
    int last_error;
    char error_msg[256];
} metac_yaml_serialization_t;

#endif // INCLUDE_METAC_YAML_SERIALIZATION_H_
```

## 2. Write Handler for libyaml

The handler bridges user buffer management with libyaml's callback model.

```c
#include <yaml.h>
#include "metac_yaml_context.h"

/**
 * @brief Custom write handler for libyaml - called by emitter when generating YAML
 * 
 * This handler receives UTF-8 encoded YAML data and writes directly to the user's buffer.
 * If the buffer fills up, overflow flag is set and handler returns 0 (fail).
 * The caller must check overflow and decide action (abort, grow buffer, reset position).
 * 
 * @param data   Pointer to the data to write
 * @param size   Number of bytes to write
 * @param ext    Opaque pointer to metac_output_t (passed via yaml_emitter_set_output)
 * @return 1 on success, 0 if buffer overflow
 */
static int metac_yaml_write_handler(void *data, unsigned char *buffer, size_t size, void *ext) {
    metac_output_t *p_out = (metac_output_t *)ext;
    
    if (p_out == NULL) {
        return 0;
    }
    
    /* Check if write would exceed buffer */
    if (p_out->current_pos + size > p_out->buffer_size) {
        p_out->overflow = 1;
        return 0;  /* Signal failure to libyaml */
    }
    
    /* Copy directly into user buffer - zero-copy from YAML's perspective */
    memcpy(p_out->p_buffer + p_out->current_pos, buffer, size);
    p_out->current_pos += size;
    
    return 1;  /* Success */
}

/**
 * @brief Initialize YAML emitter with custom write handler
 * 
 * @param p_context  The metac_yaml_context_t to initialize
 * @param p_buffer   User-allocated output buffer
 * @param buffer_size Size of buffer
 * @param wmode      Walk mode (shallow/deep)
 * @param p_tag_map  Tag map for type resolution (can be NULL)
 * @return 0 on success, negative errno on failure
 */
int metac_yaml_context_init(metac_yaml_context_t *p_context,
                            char *p_buffer,
                            size_t buffer_size,
                            metac_value_walk_mode_t wmode,
                            metac_tag_map_t *p_tag_map) {
    if (!p_context || !p_buffer || buffer_size == 0) {
        return -EINVAL;
    }
    
    memset(p_context, 0, sizeof(*p_context));
    
    /* Initialize output buffer tracking */
    p_context->output.p_buffer = p_buffer;
    p_context->output.buffer_size = buffer_size;
    p_context->output.current_pos = 0;
    p_context->output.overflow = 0;
    
    /* Initialize emitter with custom handler */
    if (!yaml_emitter_initialize(&p_context->emitter)) {
        return -EIO;
    }
    
    /* Set custom output handler - this is the key: no internal malloc */
    yaml_emitter_set_output(&p_context->emitter,
                           metac_yaml_write_handler,
                           &p_context->output);
    
    /* Configuration */
    yaml_emitter_set_canonical(&p_context->emitter, 0);  /* Flow-style preferred */
    yaml_emitter_set_indent(&p_context->emitter, 2);
    yaml_emitter_set_width(&p_context->emitter, -1);     /* No line wrapping */
    
    p_context->walk_mode = wmode;
    p_context->p_tag_map = p_tag_map;
    p_context->state.depth = 0;
    
    return 0;
}

/**
 * @brief Cleanup context and emitter
 */
void metac_yaml_context_cleanup(metac_yaml_context_t *p_context) {
    if (p_context) {
        yaml_emitter_delete(&p_context->emitter);
    }
}
```

## 3. Iterator → Emitter Main Loop

This loop demonstrates how metac's recursive iterator controls YAML event emission.

```c
#include "metac/backend/iterator.h"
#include "metac_yaml_context.h"
#include <yaml.h>

/**
 * @brief Main serialization loop: iterator yields values, we emit YAML events
 * 
 * The iterator walks through the value structure depth-first. At each step:
 * 1. We examine the current entry (field, element, etc.)
 * 2. Generate appropriate YAML event (MAPPING_START, SCALAR, SEQUENCE_START, etc.)
 * 3. Manage dependencies for recursion (struct fields, array elements)
 * 
 * @param p_val           Root value to serialize
 * @param p_context       Initialized YAML context
 * @return 0 on success, -1 if buffer overflow, other errno on failure
 */
int metac_value_to_yaml_emitter(metac_value_t *p_val, metac_yaml_context_t *p_context) {
    yaml_event_t event;
    
    if (!p_val || !p_context) {
        return -EINVAL;
    }
    
    /* Stream start */
    if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING) ||
        !yaml_emitter_emit(&p_context->emitter, &event)) {
        return -EIO;
    }
    
    /* Document start */
    if (!yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0) ||
        !yaml_emitter_emit(&p_context->emitter, &event)) {
        return -EIO;
    }
    
    /* Create iterator for value traversal */
    p_context->p_iterator = metac_new_recursive_iterator(p_val);
    if (!p_context->p_iterator) {
        return -ENOMEM;
    }
    
    /* Main event generation loop */
    for (void *p_task = metac_recursive_iterator_next(p_context->p_iterator);
         p_task != NULL;
         p_task = metac_recursive_iterator_next(p_context->p_iterator)) {
        
        metac_value_t *p_current = (metac_value_t *)p_task;
        metac_r_iter_state_t state = metac_entry_iterator_get_state(p_context->p_iterator);
        metac_kind_t kind = metac_value_final_kind(p_current, NULL);
        
        /* Determine YAML event based on entry kind and state */
        switch (kind) {
        
        case METAC_KND_struct_type:
        case METAC_KND_union_type:
        case METAC_KND_class_type: {
            switch (state) {
            case METAC_R_ITER_start: {
                /* Begin mapping for struct fields */
                if (!yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE) ||
                    !yaml_emitter_emit(&p_context->emitter, &event)) {
                    return -EIO;
                }
                
                /* Create dependencies for each field */
                metac_entry_t *p_entry = metac_value_entry(p_current);
                metac_num_t field_count = metac_entry_member_count(p_entry);
                for (metac_num_t i = 0; i < field_count; ++i) {
                    metac_entry_t *p_field = metac_entry_member(p_entry, i);
                    /* Add field as dependent task - iterator will visit it */
                    metac_recursive_iterator_create_and_append_dep(p_context->p_iterator, p_field);
                }
                
                metac_entry_iterator_set_state(p_context->p_iterator, state + 1);
                break;
            }
            
            case 1: {
                /* End mapping */
                if (!yaml_mapping_end_event_initialize(&event) ||
                    !yaml_emitter_emit(&p_context->emitter, &event)) {
                    return -EIO;
                }
                metac_recursive_iterator_done(p_context->p_iterator, NULL);
                break;
            }
            }
            break;
        }
        
        case METAC_KND_array_type: {
            switch (state) {
            case METAC_R_ITER_start: {
                /* Begin sequence for array elements */
                if (!yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE) ||
                    !yaml_emitter_emit(&p_context->emitter, &event)) {
                    return -EIO;
                }
                
                metac_entry_t *p_entry = metac_value_entry(p_current);
                metac_num_t elem_count = metac_entry_array_length(p_entry);
                for (metac_num_t i = 0; i < elem_count; ++i) {
                    metac_entry_t *p_elem_type = metac_entry_array_element_type(p_entry);
                    metac_recursive_iterator_create_and_append_dep(p_context->p_iterator, p_elem_type);
                }
                
                metac_entry_iterator_set_state(p_context->p_iterator, state + 1);
                break;
            }
            
            case 1: {
                /* End sequence */
                if (!yaml_sequence_end_event_initialize(&event) ||
                    !yaml_emitter_emit(&p_context->emitter, &event)) {
                    return -EIO;
                }
                metac_recursive_iterator_done(p_context->p_iterator, NULL);
                break;
            }
            }
            break;
        }
        
        case METAC_KND_base_type:
        case METAC_KND_enum_type: {
            /* Scalar value - no intermediate buffer, direct string from metac */
            char *p_scalar_str = metac_value_string(p_current);
            if (!p_scalar_str) {
                metac_recursive_iterator_fail(p_context->p_iterator);
                break;
            }
            
            /* Emit scalar - string reference, libyaml copies it */
            int scalar_len = strlen(p_scalar_str);
            if (!yaml_scalar_event_initialize(&event, NULL, NULL,
                                             (yaml_char_t *)p_scalar_str, scalar_len,
                                             1, 1, YAML_PLAIN_SCALAR_STYLE) ||
                !yaml_emitter_emit(&p_context->emitter, &event)) {
                free(p_scalar_str);
                return -EIO;
            }
            
            free(p_scalar_str);  /* Cleanup after libyaml has processed */
            metac_recursive_iterator_done(p_context->p_iterator, NULL);
            break;
        }
        
        default:
            /* Unknown kind - fail this task */
            metac_recursive_iterator_fail(p_context->p_iterator);
        }
        
        /* Check for buffer overflow after each event */
        if (p_context->output.overflow) {
            snprintf(p_context->error_msg, sizeof(p_context->error_msg),
                    "YAML buffer overflow at position %zu", p_context->output.current_pos);
            metac_recursive_iterator_free(p_context->p_iterator);
            return -ENOSPC;
        }
    }
    
    /* Document end */
    if (!yaml_document_end_event_initialize(&event, 0) ||
        !yaml_emitter_emit(&p_context->emitter, &event)) {
        metac_recursive_iterator_free(p_context->p_iterator);
        return -EIO;
    }
    
    /* Stream end */
    if (!yaml_stream_end_event_initialize(&event) ||
        !yaml_emitter_emit(&p_context->emitter, &event)) {
        metac_recursive_iterator_free(p_context->p_iterator);
        return -EIO;
    }
    
    metac_recursive_iterator_free(p_context->p_iterator);
    return 0;
}
```

## 4. High-Level API Functions

### 4.1 Create Serialization Context

```c
/**
 * @brief Create reusable serialization context
 * 
 * Stores value reference and settings. Iterator created on first measure() call.
 * 
 * @param p_val      Value to serialize (user-owned, must outlive context)
 * @param wmode      Walk mode: METAC_WMODE_deep or METAC_WMODE_shallow
 * @param p_tag_map  Tag map for type resolution (NULL for defaults)
 * @return Allocated context, or NULL on error
 * 
 * Usage:
 *   metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
 *       p_val, METAC_WMODE_deep, NULL);
 *   if (!serial) { perror("new"); return -1; }
 *   
 *   // ... use serial with measure() and to_buffer() ...
 *   
 *   metac_yaml_serialization_delete(serial);
 */
metac_yaml_serialization_t* metac_yaml_serialization_new(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map);

void metac_yaml_serialization_delete(metac_yaml_serialization_t *p_serial);
```

### 4.2 Measure Size (With Caching)

```c
/**
 * @brief Measure required buffer size and cache structure data
 * 
 * Single full traversal of value structure:
 *   - Creates iterator from value
 *   - Generates YAML events to 1-byte dummy buffer
 *   - Measures total size needed
 *   - CACHES iterator state for reuse
 * 
 * Can be called multiple times (recalculates if needed).
 * 
 * @param p_serial       Context from metac_yaml_serialization_new()
 * @param p_required_size (output) Bytes needed (never NULL)
 * @return 0 on success, negative errno on failure
 * 
 * Usage:
 *   size_t required;
 *   int err = metac_yaml_serialization_measure(serial, &required);
 *   if (err < 0) { fprintf(stderr, "measure failed\n"); return err; }
 *   printf("Need %zu bytes\n", required);
 */
int metac_yaml_serialization_measure(
    metac_yaml_serialization_t *p_serial,
    size_t *p_required_size);
```

### 4.3 Serialize to Buffer (Using Cached Data)

```c
/**
 * @brief Serialize value to user buffer using cached structure data
 * 
 * Uses iterator cached from measure() call - NO redundant traversal.
 * Can be called multiple times with different buffers.
 * 
 * @param p_serial        Context from metac_yaml_serialization_new()
 * @param p_buffer        User-allocated output buffer (not modified on error)
 * @param buffer_size     Buffer capacity in bytes
 * @param p_bytes_written (output, optional) Actual bytes written
 * 
 * @return 0 on success
 * @return -EINVAL if invalid args
 * @return -ENOSPC if buffer too small (size from measure() is required)
 * @return -EIO if libyaml error
 * @return other errno on failure
 * 
 * Usage:
 *   size_t required;
 *   metac_yaml_serialization_measure(serial, &required);
 *   
 *   char *buf = malloc(required);
 *   int err = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
 *   if (err == 0) {
 *       // Success - use buf[0..required-1]
 *   }
 *   free(buf);
 *
 * Multiple serializations (same value, different buffers):
 *   for (int i = 0; i < 3; i++) {
 *       metac_yaml_serialization_to_buffer(serial, bufs[i], required, NULL);
 *       // Each call reuses cached data from measure()
 *   }
 */
int metac_yaml_serialization_to_buffer(
    metac_yaml_serialization_t *p_serial,
    char *p_buffer,
    size_t buffer_size,
    size_t *p_bytes_written);
```

### 4.4 Convenience Functions (Single-Pass Dynamic)

For cases where the user is comfortable with internal memory allocation and needs a simpler API:

```c
/**
 * @brief Serialize value to dynamically allocated YAML string (single pass)
 * 
 * This is a convenience wrapper that:
 *   - Performs serialization in ONE traversal (not measure + serialize)
 *   - Dynamically allocates result buffer as needed
 *   - Returns allocated string to caller
 * 
 * Best for: Simple code, one-off serializations, non-critical paths
 * where malloc overhead is acceptable.
 * 
 * Comparison to context API:
 *   Context API (measure + to_buffer):
 *     - Pro: Full control, predictable allocations, can reuse
 *     - Con: Two function calls, explicit memory management
 *   
 *   Single-pass API:
 *     - Pro: Simple, single traversal, one call
 *     - Con: Less control, internal malloc, not reusable
 * 
 * @param p_val       Value to serialize
 * @param wmode       Walk mode: METAC_WMODE_deep or METAC_WMODE_shallow
 * @param p_tag_map   Tag map for type resolution (NULL for defaults)
 * @param p_out_size  (output) Size of allocated string (including null terminator)
 * 
 * @return Dynamically allocated YAML string (must be freed with metac_free)
 * @return NULL if allocation or serialization fails
 * 
 * @example
 *   size_t size;
 *   char *yaml = metac_value_to_yaml_string(p_val, METAC_WMODE_deep, NULL, &size);
 *   if (yaml) {
 *       fwrite(yaml, 1, size - 1, stdout);  // -1 to skip null terminator
 *       metac_free(yaml);
 *   }
 */
char* metac_value_to_yaml_string(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    size_t *p_out_size);
```

## 5. Context Structure with Caching

The improvement from previous designs is **caching intermediate data** to avoid double traversal:

```c
/**
 * @brief Reusable serialization context - caches data between measure() and serialize()
 * 
 * Eliminates redundant traversal: single measure() call, multiple serialize() calls.
 * Similar pattern to metac_memory_entry (store intermediate state for reuse).
 */
typedef struct metac_yaml_serialization {
    /* Input configuration - set by caller via new() */
    metac_value_t *p_val;           /**< Value to serialize (user-owned) */
    metac_value_walk_mode_t wmode;  /**< WMODE_deep or WMODE_shallow */
    metac_tag_map_t *p_tag_map;     /**< Type resolution map (NULL = default) */
    
    /* === CACHE FIELDS (populated by measure()) === */
    
    /**
     * Iterator state cached from measure() pass.
     * 
     * After measure() completes, this iterator is POSITIONED at the end
     * of the traversal (all work done). The next serialize() call
     * will read events from this saved state without re-traversing.
     * 
     * Lifecycle:
     *   measure()    → creates iterator, walks full tree, caches state
     *   to_buffer()  → uses cached iterator, reads events, writes to user buffer
     *   to_buffer()  → (again) reuses same cached iterator
     * 
     * This is the CORE of the zero-redundancy design.
     */
    metac_recursive_iterator_t *p_iterator_cached;
    
    /**
     * Size needed - populated by measure().
     * 
     * This is the "measured" size that user can use to allocate buffer
     * before calling to_buffer(). Since measure() has already done the work
     * of traversing and event generation, we know exactly how many bytes
     * the YAML output will be.
     */
    size_t required_size;
    
    /**
     * Flag: has measure() been called successfully?
     * 
     * to_buffer() checks this - if false, it will call measure() internally.
     * This supports the pattern: serialize without explicit measure() if desired.
     * But explicit measure() allows caller to allocate buffer before call.
     */
    int measured;
    
    /* === INTERNAL STATE (libyaml emitter) === */
    yaml_emitter_t emitter;
    
    /**
     * Current position in user buffer during serialization.
     * Used by custom write handler to track where next bytes should go.
     * Reset on each to_buffer() call.
     */
    size_t output_pos;
    
    /**
     * Marks if buffer overflowed during emitter work.
     * Custom write handler sets this if output_pos exceeds buffer_size.
     * to_buffer() returns -ENOSPC if set.
     */
    int overflow;
    
} metac_yaml_serialization_t;
```

### 5.1 Cache Lifecycle

The cache mechanism works as follows:

```
CREATE:
  serial = metac_yaml_serialization_new(p_val, wmode, p_tag_map)
  → p_iterator_cached = NULL
  → measured = 0
  
MEASURE (single traversal):
  metac_yaml_serialization_measure(serial, &size)
    1. Creates new iterator from p_val
    2. Initializes yaml_emitter_t with dummy 1-byte buffer
    3. Generates YAML events via iterator
    4. Tracks total size needed
    5. STORES iterator at p_iterator_cached  ← KEY CACHING POINT
    6. Sets required_size = total bytes
    7. Sets measured = 1
  
TO_BUFFER #1 (read from cache):
  metac_yaml_serialization_to_buffer(serial, buf1, sz, NULL)
    1. Checks measured flag
    2. Uses p_iterator_cached (no new traversal!)
    3. Initializes yaml_emitter_t with user buffer
    4. Reads events from cached iterator
    5. Custom write handler copies bytes to buf1
    6. Returns 0 if success, -ENOSPC if overflow
  
TO_BUFFER #2+ (same cache):
  metac_yaml_serialization_to_buffer(serial, buf2, sz, NULL)
    1. Resets iterator position to start
    2. Uses p_iterator_cached again
    3. Custom write handler fills buf2
    4. Can repeat infinitely (same value → different buffers)

DELETE:
  metac_yaml_serialization_delete(serial)
    1. Frees p_iterator_cached
    2. Frees other internals
    3. Value p_val unchanged (user owns it)
```

This design achieves:
- **Single traversal**: measure() does all structure analysis once
- **Reusability**: multiple serialize() calls reuse cached structure data
- **Flexibility**: measure() is optional; to_buffer() auto-measures if needed
- **Zero-malloc in serialization**: all YAML operations use user buffer only
- **Pattern familiarity**: similar to existing metac patterns (metac_memory_entry)

## 6. Usage Example

```c
#include "metac/reflect.h"
#include "metac_yaml_serialization.h"

int main() {
    struct person {
        char name[64];
        int age;
        double salary;
    };
    
    // Create reflectable value
    WITH_METAC_DECLLOC(loc,
        struct person john = {
            .name = "John Doe",
            .age = 30,
            .salary = 75000.50
        };
    )
    
    metac_value_t *p_val = METAC_VALUE_FROM_DECLLOC(loc, john);
    
    // Create reusable context
    metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
        p_val, METAC_WMODE_deep, NULL);
    if (!serial) {
        perror("new");
        return 1;
    }
    
    // Single measurement pass
    size_t required;
    int err = metac_yaml_serialization_measure(serial, &required);
    if (err < 0) {
        fprintf(stderr, "Measure failed: %s\n", strerror(-err));
        metac_yaml_serialization_delete(serial);
        return 1;
    }
    
    printf("YAML needs %zu bytes\n", required);
    
    // Allocate user buffer
    char *buffer = malloc(required);
    if (!buffer) {
        perror("malloc");
        metac_yaml_serialization_delete(serial);
        return 1;
    }
    
    // Serialize using cached data (NO re-traversal!)
    size_t written;
    err = metac_yaml_serialization_to_buffer(serial, buffer, required, &written);
    if (err == 0) {
        printf("YAML output (%zu bytes):\n%.*s\n", written, (int)written, buffer);
        
        // Can reuse context to serialize to multiple destinations
        FILE *file = fopen("output.yaml", "w");
        if (file) {
            fwrite(buffer, 1, written, file);
            fclose(file);
        }
    } else {
        fprintf(stderr, "Serialization error: %s\n", strerror(-err));
    }
    
    free(buffer);
    metac_yaml_serialization_delete(serial);
    metac_value_delete(p_val);
    return 0;
}
```

## 7. Key Design Decisions

### 7.1 Cached Context Instead of Direct API
**Problem**: Original design with `metac_value_yaml_required_size()` + `metac_value_to_yaml_buffer()` required double traversal:
- Call 1: measure required size (full tree walk, discard results)
- Call 2: serialize to buffer (full tree walk again, write output)

**Solution**: Context object stores cached iterator state:
- `measure()`: Single traversal, cache iterator state at completion
- `to_buffer()`: Read from cached iterator, no re-traversal
- Multiple `to_buffer()` calls: Reuse same cache (e.g., serialize to file AND network)

**Benefit**: Eliminates redundant work, especially for large nested structures. Similar to established metac patterns like `metac_memory_entry`.

### 7.2 Zero-Malloc in metac
- User provides output buffer via `to_buffer()`
- `metac_yaml_write_handler` copies YAML bytes directly to that buffer
- No intermediate string allocations within metac layer during emission
- Iterator works with user's value (doesn't clone) - read-only access only

### 7.3 Event-Driven Architecture
- Iterator yields one value/field at a time
- Each yield generates one YAML event
- State machine tracks nesting (struct fields → mapping, array elements → sequence)
- Dependencies managed by iterator (struct fields, array elements)

### 7.4 Custom libyaml Handler
- `yaml_emitter_set_output()` registers callback for every data output
- Callback receives UTF-8 encoded YAML bytes
- We copy directly to user's buffer without intermediate buffering
- If buffer fills, handler sets overflow flag; caller retries with larger buffer

### 7.5 Overflow Handling
- Write handler detects potential overflow before writing
- Returns 0 (fail) to libyaml to abort gracefully
- Caller checks overflow flag and can:
  - Allocate larger buffer and retry with same context
  - Stream to file/network using smaller chunks
  - Use `measure()` result to pre-allocate correct size

### 7.6 Platform Compatibility
- Uses standard `yaml.h` (libyaml - cross-platform)
- Works on Linux, macOS, Windows (wherever libyaml available)
- No platform-specific code in metac serialization layer
- DWARF extraction platform-specific (handled separately in build system)


