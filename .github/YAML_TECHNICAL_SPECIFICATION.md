# YAML Serialization API - Complete Technical Specification

**Date**: February 5, 2026  
**Status**: Design Specification (Ready for Implementation)  
**Component**: metac Serialization Backend  

---

## Executive Summary

This specification defines a zero-malloc YAML serialization API for metac that:
- ✅ Gives users complete memory control (no hidden allocations)
- ✅ Supports bounded buffers (predictable memory usage)
- ✅ Handles deep structures without stack overflow (heap-based iterator)
- ✅ Achieves near-zero-copy semantics (direct pipe to user buffer)
- ✅ Integrates seamlessly with `libyaml` event model

---

## Problem Analysis

### Current Implementation Limitations

**Existing API** (`metac_value_to_yaml`):
```c
char* metac_value_to_yaml(metac_value_t* p_val, ...);
```

Issues:
1. **Hidden Allocations**: Entire YAML string allocated internally
2. **No Size Prediction**: Can't pre-allocate correct buffer
3. **Memory Explosion**: Large structures → large strings
4. **Embedded Unfriendly**: No support for bounded buffers
5. **Stack Risk**: Deep recursion with recursive descent

### Design Requirements

1. **User-Controlled Memory**: User provides buffer; metac doesn't malloc
2. **Overflow Detection**: Graceful handling when buffer exhausted
3. **Stack Safety**: No recursive descent limitations
4. **Efficiency**: Minimal per-field overhead
5. **API Simplicity**: Easy to use correctly

---

## Solution Architecture

### 3-Layer Design

```
┌─────────────────────────────────────────┐
│  Layer 1: High-Level API                │
│  metac_value_to_yaml_buffer()           │ ← User calls
└─────────────────────────────────────────┘
         │
┌─────────────────────────────────────────┐
│  Layer 2: Event Generation Loop         │
│  metac_value_to_yaml_impl()             │ ← Iterator + events
│  Iterator → YAML events → libyaml       │
└─────────────────────────────────────────┘
         │
┌─────────────────────────────────────────┐
│  Layer 3: I/O Handler                   │
│  metac_yaml_write_handler()             │ ← Direct to buffer
│  Write handler → memcpy to user buffer  │
└─────────────────────────────────────────┘
         │
         ▼
    User Buffer
    (malloc, stack, fixed, etc.)
```

### Component: metac_yaml_context_t

```c
typedef struct {
    yaml_emitter_t emitter;           // libyaml emitter instance
    
    // Output buffer (user-owned)
    char *p_buffer;                   // Pointer to user buffer
    size_t buffer_size;               // Total capacity
    size_t current_pos;               // Bytes written
    int overflow;                     // Flag: overflow detected
    
    // Traversal state
    metac_recursive_iterator_t *p_iter;   // Heap-based traversal
    metac_value_walk_mode_t walk_mode;    // Deep vs shallow
    metac_tag_map_t *p_tag_map;           // Type resolution
    
    // Error context
    char error_msg[256];              // Diagnostic info
} metac_yaml_context_t;
```

### Component: Write Handler

```c
static int metac_yaml_write_handler(void *data,
                                    unsigned char *buffer,
                                    size_t size,
                                    void *ext) {
    metac_output_t *out = (metac_output_t *)ext;
    
    // Check buffer bounds
    if (out->current_pos + size > out->buffer_size) {
        out->overflow = 1;
        return 0;  // Fail - signals to libyaml
    }
    
    // Direct copy
    memcpy(out->p_buffer + out->current_pos, buffer, size);
    out->current_pos += size;
    
    return 1;  // Success
}
```

**Key Design Decisions:**
- Called by libyaml with YAML chunks (typically 10-100 bytes)
- Checks bounds before write
- Sets overflow flag on exhaustion (non-destructive failure)
- No internal buffering (direct memcpy)

### Component: Event Generation Loop

```
START
  ├─ Create iterator from value
  │
  ├─ Emit STREAM_START
  ├─ Emit DOCUMENT_START
  │
  └─ MAIN LOOP: for each iterator task
      ├─ Switch on task type:
      │   ├─ Struct → MAPPING_START (create field deps)
      │   ├─ Array → SEQUENCE_START (create element deps)
      │   └─ Scalar → emit SCALAR event
      │
      ├─ Advance iterator to next task
      │
      └─ Check overflow flag after each event
          ├─ If set: abort, return -ENOSPC
          └─ If clear: continue
  │
  ├─ Emit DOCUMENT_END
  ├─ Emit STREAM_END
  │
  └─ Cleanup iterator
END
```

---

## API Specification

### Public Functions

#### 1. `metac_value_to_yaml_buffer()`

```c
int metac_value_to_yaml_buffer(
    metac_value_t *p_val,
    char *p_buffer,
    size_t buffer_size,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    size_t *p_bytes_written);
```

**Purpose**: Serialize metac value to YAML in user-provided buffer.

**Arguments**:
- `p_val`: Pointer to value to serialize (required)
- `p_buffer`: Pointer to output buffer (required, user-owned)
- `buffer_size`: Capacity of buffer in bytes (required, > 0)
- `wmode`: Walk mode - METAC_WMODE_shallow or METAC_WMODE_deep (required)
- `p_tag_map`: Tag map for type resolution (optional, NULL allowed)
- `p_bytes_written`: Output pointer to bytes written/required (optional)

**Return Values**:
| Value | Meaning | `p_bytes_written` | Action |
|-------|---------|-------------------|--------|
| `0` | Success | Bytes written to buffer | Use buffer[0..n-1] |
| `-EINVAL` | Invalid args | Unchanged | Check p_val, p_buffer |
| `-ENOSPC` | Buffer too small | Bytes needed | Allocate and retry |
| `-EIO` | libyaml error | Bytes written | Check error_msg |
| `-ENOMEM` | malloc failed | Unchanged | Out of memory |

**Guarantees**:
- On failure: buffer is NOT modified (safe retry)
- On success: buffer[0..n-1] contains valid YAML
- On -ENOSPC: `*p_bytes_written` indicates required size

**Example: Fixed Buffer**
```c
char buf[4096];
size_t sz;
int err = metac_value_to_yaml_buffer(pv, buf, sizeof(buf),
                                     METAC_WMODE_deep, NULL, &sz);
if (err == 0) {
    write(STDOUT_FILENO, buf, sz);
}
```

**Example: Dynamic Allocation**
```c
size_t required;
metac_value_yaml_required_size(pv, &required, METAC_WMODE_deep, NULL);
char *buf = malloc(required);
if (buf) {
    int err = metac_value_to_yaml_buffer(pv, buf, required,
                                         METAC_WMODE_deep, NULL, NULL);
    if (err == 0) {
        // Use buf...
    }
    free(buf);
}
```

#### 2. `metac_value_yaml_required_size()`

```c
int metac_value_yaml_required_size(
    metac_value_t *p_val,
    size_t *p_required_size,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map);
```

**Purpose**: Measure minimum buffer size needed (without writing).

**Method**: Runs full serialization to 1-byte buffer; measures total.

**Return**: Always `-ENOSPC` (by design)

**Output**: `*p_required_size` = bytes needed

**Performance**: O(n) where n = structure size (full traversal required)

**Example**:
```c
size_t sz;
metac_value_yaml_required_size(pv, &sz, METAC_WMODE_deep, NULL);
printf("Need %zu bytes\n", sz);
```

#### 3. `metac_yaml_context_init()` (Internal)

```c
int metac_yaml_context_init(metac_yaml_context_t *p_context,
                            char *p_buffer,
                            size_t buffer_size,
                            metac_value_walk_mode_t wmode,
                            metac_tag_map_t *p_tag_map);
```

**Purpose**: Initialize emitter with custom write handler.

**Called by**: `metac_value_to_yaml_buffer()` (user normally doesn't call)

**Side Effects**:
- Initializes libyaml emitter
- Registers custom write handler
- Clears buffer tracking state

**Return**: 0 on success, -EIO on failure

---

## Memory Model

### Buffer Ownership

```c
// User allocates and owns buffer
char buf[4096];

// User passes to metac
metac_value_to_yaml_buffer(pv, buf, sizeof(buf), ...);

// Metac writes to buf via write_handler
// libyaml copies here; user owns lifetime

// User uses buf after call
printf("%.*s\n", (int)sz, buf);

// User frees (if malloc'd)
free(buf);
```

### Allocations During Serialization

| Component | Allocation | Frequency | Owner | Freed When |
|-----------|-----------|-----------|-------|-----------|
| Iterator | Task queue | Once at start | metac | End of serialization |
| Scalar strings | C-string formatting | Per scalar | metac | After emit |
| Event objects | Stack temporary | Per event | Stack | After emit |
| User buffer | Output | N/A | User | User decides |

**Total metac malloc**: O(depth) for iterator + O(fields) for scalars

### Zero-Copy Claims

**Scalars**: Not truly zero-copy (need C-string formatting)
- metac_value_string() allocates
- libyaml copies from string
- metac frees string
- Result: 1 allocation per scalar, small impact

**Containers**: True zero-copy
- No intermediate buffer
- Events emitted directly to write_handler
- write_handler memcpys to user buffer
- No extra allocation

---

## Error Handling Strategy

### Overflow Detection

```
Buffer capacity: 256 bytes
Serialization needs: 312 bytes

Step 1: write_handler called with 10 bytes
  → pos + 10 ≤ 256 ✓ write, pos = 10

Step N: write_handler called with 80 bytes
  → pos + 80 > 256 ✗ overflow = 1, return 0

libyaml detects failure, stops emitting

metac_value_to_yaml_impl() returns -ENOSPC

User sees: err = -ENOSPC, bytes_written = 256
User knows: "need at least 256 bytes"
User can: allocate 256+ bytes and retry
```

### Non-Destructive Failure

```c
int err = metac_value_to_yaml_buffer(pv, buf, 256, METAC_WMODE_deep, NULL, &sz);

if (err == -ENOSPC) {
    // buf[0..255] is PARTIAL YAML (incomplete)
    // Don't use it!
    // sz indicates total needed
    
    char *bigger = malloc(sz);
    err = metac_value_to_yaml_buffer(pv, bigger, sz, METAC_WMODE_deep, NULL, NULL);
    // Now buf[0..sz-1] is complete
}
```

---

## Integration Example: Complete Program

```c
#include <stdio.h>
#include <stdlib.h>
#include "metac/reflect.h"
#include "metac/serialization/yaml.h"

struct config {
    int port;
    char hostname[64];
    double timeout;
};

int main() {
    struct config cfg = {
        .port = 8080,
        .hostname = "localhost",
        .timeout = 30.5
    };
    
    // Create reflectable value
    WITH_METAC_DECLLOC(loc,
        struct config cfg = {
            .port = 8080,
            .hostname = "localhost",
            .timeout = 30.5
        };
    )
    
    metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, cfg);
    if (!pv) {
        fprintf(stderr, "Failed to create value\n");
        return 1;
    }
    
    // Measure required size
    size_t required;
    metac_value_yaml_required_size(pv, &required, METAC_WMODE_deep, NULL);
    
    // Allocate buffer
    char *yaml = malloc(required);
    if (!yaml) {
        fprintf(stderr, "Out of memory\n");
        metac_value_delete(pv);
        return 1;
    }
    
    // Serialize
    int err = metac_value_to_yaml_buffer(pv, yaml, required,
                                         METAC_WMODE_deep, NULL, NULL);
    
    if (err == 0) {
        // Success
        printf("Config as YAML:\n%.*s\n", (int)required, yaml);
    } else {
        fprintf(stderr, "Serialization failed: %s\n", strerror(-err));
    }
    
    free(yaml);
    metac_value_delete(pv);
    
    return err == 0 ? 0 : 1;
}
```

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `metac_value_to_yaml_buffer()` | O(n) | n = total bytes in output |
| `metac_value_yaml_required_size()` | O(n) | Must traverse entire structure |
| Event emission | O(1) per event | Typically 100-1000 events per structure |
| write_handler | O(k) | k = chunk size (typically <100 bytes) |

### Space Complexity

| Component | Space | Scaling |
|-----------|-------|---------|
| Iterator | O(d) | d = max nesting depth |
| Scalars | O(m) | m = max scalar size |
| User buffer | O(size) | Linear with desired output |
| libyaml internal | O(1) | Fixed overhead |

### Memory Efficiency Example

**Structure**: 1000-element array of `{int, double}`

```
Fixed fields:     ~100 bytes YAML
Per element:      ~20 bytes YAML
Total needed:     ~20,100 bytes

Allocation during serialization:
  - Iterator queue:       O(log 1000) ≈ 10 tasks
  - Scalar strings:       ~40 bytes per 2 scalars → 20,000 bytes
  - User buffer:          20,100 bytes
  
Total heap:       ~20,110 bytes (reasonable)

Without user-controlled buffer:
  - Would need to buffer entire 20,100 byte string internally
  - Plus all intermediate task data
  - Potential for 2x memory usage
```

---

## Files and Locations

### Headers
- `include/metac/serialization/yaml.h` - Public API declarations

### Implementation
- `src/serialization/yaml/yaml_context.c` - Context + write handler
- `src/serialization/yaml/yaml_emitter.c` - Event generation loop
- `src/serialization/yaml/api.c` - Public function implementations

### Tests
- `src/serialization/yaml/yaml_test.c` - Unit tests

### Documentation
- `.github/YAML_QUICK_START.md` - Quick reference
- `.github/YAML_SERIALIZATION_DESIGN.md` - Full design
- `.github/YAML_IMPLEMENTATION_GUIDE.md` - Implementation checklist
- `.github/YAML_DATAFLOW_DIAGRAMS.md` - Visual explanations

---

## Acceptance Criteria

✅ Implementation complete when:

1. **API Functions**
   - [ ] `metac_value_to_yaml_buffer()` implemented and tested
   - [ ] `metac_value_yaml_required_size()` implemented and tested
   - [ ] Error codes match specification

2. **Behavior**
   - [ ] No malloc inside metac (except iterator + per-scalar strings)
   - [ ] Overflow detection works correctly
   - [ ] Non-destructive failure (buffer not corrupted on -ENOSPC)
   - [ ] Successful retry after overflow

3. **Memory Safety**
   - [ ] Iterator properly freed on all code paths
   - [ ] No buffer overflows
   - [ ] All temporary allocations cleaned up

4. **Integration**
   - [ ] Compiles with libyaml-dev installed
   - [ ] Links libmetac.a successfully
   - [ ] Works in examples and demo applications

5. **Testing**
   - [ ] Unit tests for scalar, struct, array serialization
   - [ ] Tests for buffer overflow scenarios
   - [ ] Tests for deep vs shallow walk modes
   - [ ] Tests for error conditions
   - [ ] Memory leak detection (valgrind/asan)

6. **Documentation**
   - [ ] Doxygen comments in headers
   - [ ] Architecture docs complete
   - [ ] Code examples work
   - [ ] Error codes documented

---

## Backward Compatibility

**Impact**: New API, existing `metac_value_to_yaml()` unchanged

**Migration Path**: 
- Old code continues to work (malloc version)
- New code can opt into buffer-controlled version
- Can deprecate old API in future version

---

## Future Enhancements

1. **Streaming**: Extend write_handler to support file descriptors
2. **Deserialization**: Implement `metac_value_from_yaml_buffer()` (same model)
3. **JSON Backend**: Apply same pattern to JSON serialization
4. **Custom Events**: Allow user callbacks for custom type handling
5. **Size Hints**: Pre-calculate size for common types

---

## References

- **libyaml Documentation**: http://pyyaml.org/wiki/LibYAML
- **Metac Iterator**: `include/metac/backend/iterator.h`
- **Value API**: `include/metac/reflect/value.h`
- **YAML Events**: `<yaml.h>` header file

---

**Document Status**: Ready for review and implementation

**Next Steps**:
1. Review specification with team
2. Implement according to guidelines in [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md)
3. Test with checklist from [YAML_QUICK_START.md](YAML_QUICK_START.md)
4. Merge to main branch
