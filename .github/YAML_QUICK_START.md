# YAML Serialization API - Quick Reference

## Problem Statement

**Before**: 
- `metac_value_to_yaml()` allocates entire YAML string internally via `malloc`
- No control over memory consumption
- Not suitable for embedded systems or bounded buffers

**Now**: 
- User provides buffer
- metac acts as event provider to `libyaml`
- Zero internal malloc in metac during serialization
- User owns all memory

---

## Core API - Reusable Context Approach

### 1. Create Serialization Context

```c
// Opaque serialization context (stores intermediate data, cached iterators, etc.)
typedef struct metac_yaml_serialization metac_yaml_serialization_t;

// Create context
metac_yaml_serialization_t* metac_yaml_serialization_new(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map);

// Cleanup
void metac_yaml_serialization_delete(metac_yaml_serialization_t *p_serial);
```

### 2. Measure and Cache Size (One Pass)

```c
// Measure required size - internally caches structure information
int metac_yaml_serialization_measure(
    metac_yaml_serialization_t *p_serial,
    size_t *p_required_size);

// Returns: 0 on success, negative errno on failure
// After call: p_required_size contains exact bytes needed
```

**Example:**
```c
struct point { int x, y; } p = {10, 20};
WITH_METAC_DECLLOC(loc, struct point p = {10, 20};)
metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, p);

// Create context (once)
metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
    pv, METAC_WMODE_deep, NULL);

// Measure size (one pass - caches internal structure)
size_t required;
int err = metac_yaml_serialization_measure(serial, &required);
if (err < 0) {
    fprintf(stderr, "Measurement failed\n");
    goto cleanup;
}

// Now we know exact size - allocate once
char *buf = malloc(required);

// Serialize using cached data (second pass, no redundant work)
err = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
if (err == 0) {
    printf("YAML (%zu bytes):\n%.*s\n", required, (int)required, buf);
}

free(buf);
cleanup:
    metac_yaml_serialization_delete(serial);
    metac_value_delete(pv);
```

### 3. Serialize to Buffer (Using Cached Data)

```c
// Serialize to buffer - uses cached iterator from measure() call
int metac_yaml_serialization_to_buffer(
    metac_yaml_serialization_t *p_serial,
    char *p_buffer,
    size_t buffer_size,
    size_t *p_bytes_written);

// Returns: 0 on success, -ENOSPC if buffer too small, other errno on failure
// Note: Can be called multiple times with different buffers
```

**Error Codes:**
- `0` - Success
- `-EINVAL` - Invalid context or buffer
- `-ENOSPC` - Buffer too small (use p_bytes_written from measure())
- `-EIO` - libyaml error

---

## Architecture in 30 Seconds

```
User Buffer
    ↑
    │ memcpy (write_handler)
    │
libyaml emitter
    ↑
    │ yaml_event_t (MAPPING_START, SCALAR, etc.)
    │
metac Iterator
    ↑
    │ tasks (struct fields, array elements)
    │
metac_value_t (your data structure)
```

**Flow:**
1. Create iterator from value
2. For each task from iterator:
   - Examine type (struct/array/scalar)
   - Emit YAML event via libyaml
3. libyaml calls write_handler
4. write_handler copies to user buffer
5. Iterator proceeds to next field/element

---

## Typical Usage Patterns

### Pattern 1: Measure Once, Allocate, Serialize

**Best for:** Normal case - know size upfront
```c
metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
    pv, METAC_WMODE_deep, NULL);

size_t required;
metac_yaml_serialization_measure(serial, &required);  // First pass

char *buf = malloc(required);
int err = metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
if (err == 0) {
    // Success - use buf[0..required-1]
}
free(buf);
metac_yaml_serialization_delete(serial);
```

### Pattern 2: Try Fixed Buffer, Fail Gracefully

**Best for:** Stack buffer, retry with malloc if needed
```c
metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
    pv, METAC_WMODE_deep, NULL);

size_t required;
metac_yaml_serialization_measure(serial, &required);

if (required <= sizeof(stack_buf)) {
    // Fits in stack
    int err = metac_yaml_serialization_to_buffer(serial, stack_buf,
                                                 sizeof(stack_buf), NULL);
} else {
    // Need heap allocation
    char *heap_buf = malloc(required);
    int err = metac_yaml_serialization_to_buffer(serial, heap_buf,
                                                 required, NULL);
    free(heap_buf);
}

metac_yaml_serialization_delete(serial);
```

### Pattern 3: Serialize to Multiple Buffers (Same Value)

**Best for:** Write same YAML to file AND network (reuse cached data!)
```c
metac_yaml_serialization_t *serial = metac_yaml_serialization_new(
    pv, METAC_WMODE_deep, NULL);

size_t required;
metac_yaml_serialization_measure(serial, &required);

char *buf = malloc(required);

// First serialization - to file
metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
write_to_file(buf, required);

// Second serialization - to network (reuses cached data!)
metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
send_over_network(buf, required);

free(buf);
metac_yaml_serialization_delete(serial);
```

### Pattern 4: Single-Pass Dynamic (Fire-and-Forget)

**Best for:** Simple code, when malloc overhead is acceptable, one-off serializations
```c
// Everything in ONE pass - no measure(), no explicit allocations from user
size_t size;
char *yaml_str = metac_value_to_yaml_string(
    pv, METAC_WMODE_deep, NULL, &size);

if (yaml_str) {
    fwrite(yaml_str, 1, size, stdout);
    metac_free(yaml_str);  // metac-allocated, so metac frees it
}
```

**Why this pattern?**
- Single traversal (not two like Pattern 1)
- Internally allocates buffer, no user malloc needed
- Simpler code for non-performance-critical paths
- Similar to how libraries like cJSON work

**Implementation note:**
- `metac_value_to_yaml_string()` walks structure once, dynamically grows buffer
- Returns dynamically allocated string (ownership passes to caller)
- Caller must call `metac_free()` to release

---

## Why This Design?

### Problem with Naive Approach (Redundant Traversals)
```
BEFORE (two passes):
1. Call metac_value_yaml_required_size(pv, &sz) 
   → Full traversal: create iterator, generate events, discard
   → Only get size

2. Call metac_value_to_yaml_buffer(pv, buf, sz)
   → Full traversal AGAIN: create new iterator, generate events again
   → Finally write to buffer
```

**Issue**: Double work - traverse structure twice!

### Solution: Reusable Context
```
NOW (cached approach):
1. Create context: metac_yaml_serialization_new(pv, ...)
   → Stores: value pointer, walk mode, tag map

2. Measure: metac_yaml_serialization_measure(serial, &sz)
   → Single traversal: create iterator, cache structure info
   → Get exact size + cache internal iterator/state

3. Serialize: metac_yaml_serialization_to_buffer(serial, buf, sz)
   → Use CACHED iterator and structure data
   → No redundant traversal
   → Can be called multiple times with different buffers
```

**Benefit**: Single traversal, reusable data, optional multiple serializations

---

## Architecture Note: Shared Core Code

### Both APIs Use Same Implementation

**Important**: The convenience function (`metac_value_to_yaml_string`) is NOT a separate implementation. 

It's a thin wrapper (~28 lines) that calls the exact same core functions:

```
User chooses API:

Option 1: Context API (explicit)          Option 2: Convenience (implicit)
┌──────────────────────────┐             ┌──────────────────────────┐
│ metac_yaml_serialization │             │ metac_value_to_yaml_     │
│    _new()                │             │     string()             │
│ metac_yaml_serialization │             │                          │
│    _measure()            │             │ (calls options 1 internally)
│ metac_yaml_serialization │             └──────────────────────────┘
│    _to_buffer()          │                      ↓
│ metac_yaml_serialization │             Same core functions!
│    _delete()             │
└──────────────────────────┘
         ↓
    ┌────────────────────┐
    │  Shared Core Code  │
    │  (yaml_context.c,  │
    │   yaml_emitter.c)  │
    │  ~250 lines        │
    └────────────────────┘
```

### Benefits for Users

1. **Identical behavior**: Both APIs produce identical YAML
2. **Bug fixes automatic**: Fix core once, both APIs benefit
3. **Performance same**: No overhead from convenience wrapper
4. **Easy maintenance**: Single source of truth

### Code Statistics

- **Core implementation**: ~250 lines (shared)
- **Convenience wrapper**: ~28 lines (90% code reuse)
- **Total for both APIs**: ~278 lines

**vs separate implementations**: Would be ~500+ lines with duplicate code

---

## Error Codes

| Code | Meaning | Action |
|------|---------|--------|
| `0` | Success | Use buffer[0..bytes_written] |
| `-EINVAL` | Bad args | Check p_val, p_buffer not NULL |
| `-ENOSPC` | Buffer too small | Allocate `bytes_written` bytes, retry |
| `-EIO` | libyaml error | Likely corruption or internal error |
| `-ENOMEM` | malloc failed | Out of memory |

---

## Build Integration

### Makefile

```makefile
# Link libyaml
LDFLAGS += $(shell pkg-config --libs yaml-0.1)
CFLAGS += $(shell pkg-config --cflags yaml-0.1)

# Source files
libmetac_reflect += \
    src/serialization/yaml/yaml_context.c \
    src/serialization/yaml/yaml_emitter.c \
    src/serialization/yaml/api.c
```

### Dependencies

- **libyaml**: `apt install libyaml-dev` (Ubuntu)
- **macOS**: `brew install libyaml`
- **Windows**: vcpkg or pre-built

---

## Design Rationale

### Why Custom Write Handler?

```c
// BEFORE (malloc internally):
char *yaml = metac_value_to_yaml(pv);  // metac allocates
// Problem: No control, could be huge

// AFTER (user buffer + write_handler):
char buf[4096];
metac_value_to_yaml_buffer(pv, buf, sizeof(buf), ...);
// Benefit: Predictable memory, no hidden malloc
```

### Why Iterator State Machine?

```c
// BEFORE: Recursive descent (limited by stack)
void emit_recursive(metac_value_t *pv) {
    if (is_struct(pv)) {
        for each member:
            emit_recursive(member);  // Stack grows!
    }
}

// AFTER: Heap-based task queue (unlimited depth)
metac_recursive_iterator_t *iter = metac_new_recursive_iterator(pv);
for (void *task = metac_recursive_iterator_next(iter); task; ...) {
    // Handle one level at a time
    // No recursion = no stack limit
}
```

### Why Zero-Copy Ideal?

**Goal**: Minimize allocations per structure size

- Scalars: 1 malloc (for C-string formatting) per scalar
- Containers: 0 malloc (just events)
- Total: Proportional to # fields, not structure size

**Example**: Serializing 1000-element array
- BEFORE: Entire array buffered in metac (huge)
- AFTER: Events emitted per element, stored in user buffer directly

---

## Documentation Links

- **Architecture**: [YAML_SERIALIZATION_DESIGN.md](YAML_SERIALIZATION_DESIGN.md)
- **Implementation**: [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md)
- **Data Flows**: [YAML_DATAFLOW_DIAGRAMS.md](YAML_DATAFLOW_DIAGRAMS.md)

---

## FAQ

**Q: Can I serialize to a file descriptor?**
A: Not directly. Serialize to buffer, then write buffer to fd. Could extend write_handler to support fd later.

**Q: What if value structure is really deep?**
A: Iterator handles it via heap tasks, not stack recursion. No depth limit.

**Q: What about thread safety?**
A: Each thread needs its own context & buffer. metac doesn't use global state.

**Q: Can I interrupt serialization?**
A: From write_handler: return 0 to signal failure. Iterator will propagate error.

**Q: What about YAML from deserialization (from_yaml)?**
A: Currently uses malloc. Can be optimized similarly if needed.
