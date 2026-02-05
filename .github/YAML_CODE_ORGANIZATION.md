# YAML Serialization: Code Organization & Shared Architecture

## Problem Solved

When designing an API with multiple entry points (Context API + Convenience API), the challenge is avoiding code duplication while maintaining clean interfaces.

**Solution**: Single shared core implementation with thin wrappers.

---

## File Organization

### `src/serialization/yaml/` Directory Structure

```
src/serialization/yaml/
├── yaml_handler.c          [150 lines]  ← libyaml write handler
│   └── metac_yaml_write_handler()
│       - Custom callback for libyaml
│       - Direct buffer copy (zero malloc)
│       - Overflow detection
│
├── yaml_context.c          [100 lines]  ← Context lifecycle
│   ├── metac_yaml_serialization_new()
│   │   - Allocate context structure
│   │   - Initialize pointers
│   ├── metac_yaml_serialization_delete()
│   │   - Free iterator cache
│   │   - Free internals
│   └── metac_yaml_context_init() [internal]
│       - Setup emitter instance
│
├── yaml_events.c           [200+ lines] ← Event generation (core logic)
│   ├── metac_yaml_serialization_measure() [KEY FUNCTION]
│   │   - Create iterator
│   │   - Full traversal to dummy buffer
│   │   - CACHE iterator state ← caching happens here
│   │   - Return measured size
│   │
│   ├── metac_yaml_serialization_to_buffer() [KEY FUNCTION]
│   │   - Initialize emitter with user buffer
│   │   - Use cached iterator from measure()
│   │   - Generate YAML events
│   │   - Write to user buffer
│   │
│   └── Internal helpers:
│       - emit_event_for_kind() - per-type event generation
│       - handle_struct_fields() - traverse struct members
│       - handle_array_elements() - traverse array items
│
├── api.c                   [~30 lines]  ← Thin wrappers (convenience)
│   ├── metac_value_to_yaml_string() [WRAPPER]
│   │   1. Call metac_yaml_serialization_new()    [shared]
│   │   2. Call metac_yaml_serialization_measure() [shared]
│   │   3. malloc() buffer                        [new]
│   │   4. Call metac_yaml_serialization_to_buffer() [shared]
│   │   5. Call metac_yaml_serialization_delete()  [shared]
│   │   → Return allocated string
│   │
│   └── metac_yaml_free()
│       - Free allocated string
│
└── yaml_test.c             [~100 lines] ← Test suite (covers both APIs)
    ├── test_yaml_context_api_simple_struct()
    ├── test_yaml_single_pass_convenience()
    └── test_yaml_context_reuse()
```

### Code Dependency Graph

```
┌──────────────────────────────────────┐
│  Public API Layer                    │
├──────────────────────────────────────┤
│                                      │
│  Context API:                        │
│  - metac_yaml_serialization_new()    │
│  - metac_yaml_serialization_measure()├─┐
│  - metac_yaml_serialization_to_buffer()├─┐
│  - metac_yaml_serialization_delete()│  │
│                                      │  │
│  Convenience API:                    │  │
│  - metac_value_to_yaml_string()     ├──┤ (just calls context API)
│  - metac_yaml_free()                 │  │
│                                      │  │
└──────────────────────────────────────┘  │
         │                                │
         │  (different entry points)      │
         │  (same core below)             │
         ↓                                │
┌──────────────────────────────────────┐  │
│  Core Implementation Layer           │  │
├──────────────────────────────────────┤  │
│                                      │  │
│  yaml_events.c:                      │←─┘
│  - metac_yaml_serialization_measure()
│    ├─ Create iterator
│    ├─ Generate events → dummy buffer
│    ├─ CACHE iterator
│    └─ Return size
│  
│  - metac_yaml_serialization_to_buffer()
│    ├─ Initialize emitter
│    ├─ Use cached iterator
│    ├─ Generate events
│    └─ Write to user buffer
│
│  - emit_event_for_kind() [helper]
│  - handle_* functions [helpers]
│
│  yaml_context.c:
│  - Context allocation/deallocation
│  - Emitter setup
│
│  yaml_handler.c:
│  - libyaml write callback
│  - Buffer management
│
└──────────────────────────────────────┘
         │
         ↓
┌──────────────────────────────────────┐
│  External Dependencies               │
├──────────────────────────────────────┤
│                                      │
│  - metac/backend/iterator.h          │
│    (recursive iterator)              │
│                                      │
│  - metac/backend/value.h             │
│    (value introspection)             │
│                                      │
│  - yaml.h (libyaml)                  │
│    (YAML emitter)                    │
│                                      │
└──────────────────────────────────────┘
```

---

## Implementation Details: How Sharing Works

### Example: Serializing a Struct

**Scenario**: User wants YAML of a struct value. Shows how both APIs execute same code.

#### Context API Execution
```c
// User code
metac_yaml_serialization_t *serial = 
    metac_yaml_serialization_new(p_val, METAC_WMODE_deep, NULL);

size_t required;
metac_yaml_serialization_measure(serial, &required);
    ↓
    [yaml_events.c - SHARED]
    1. metac_recursive_iterator_t *iter = create_iterator(p_val)
    2. yaml_event_t event = {...}
    3. for each field in struct:
           emit_event_for_kind(field_type) → YAML event
       4. yaml_emitter_emit(&emitter, &event) → write to dummy buffer
    5. Track total bytes written
    6. CACHE: serial->p_iterator_cached = iter
    7. Return: *required = total_bytes

char buf[1024];
metac_yaml_serialization_to_buffer(serial, buf, 1024, NULL);
    ↓
    [yaml_events.c - SHARED]
    1. Initialize emitter with user buffer
    2. Use serial->p_iterator_cached (already walked structure!)
    3. for each cached event:
           yaml_emitter_emit(...) → write to user buffer
    4. Return success

metac_yaml_serialization_delete(serial);
    ↓
    [yaml_context.c - SHARED]
    free(serial->p_iterator_cached)
    free(serial)
```

#### Convenience API Execution
```c
// User code
size_t size;
char *yaml = metac_value_to_yaml_string(p_val, METAC_WMODE_deep, NULL, &size);
    ↓
    [api.c - THIN WRAPPER, ~28 lines]
    
    serial = metac_yaml_serialization_new(...)
        ↓ SHARED CODE (yaml_context.c)
    
    metac_yaml_serialization_measure(serial, &required)
        ↓ SHARED CODE (yaml_events.c)
        [Same execution as Context API above]
    
    buf = malloc(required + 1)
        ↓ NEW CODE (only malloc)
    
    metac_yaml_serialization_to_buffer(serial, buf, required, &written)
        ↓ SHARED CODE (yaml_events.c)
        [Same execution as Context API above]
    
    buf[written] = '\0'
    metac_yaml_serialization_delete(serial)
        ↓ SHARED CODE (yaml_context.c)
    
    return buf
```

**Result**: Identical YAML output, identical behavior, single code path!

---

## Maintenance Scenarios

### Scenario 1: Bug Fix - Unicode in Nested Structs

**Bug**: "Chinese characters corrupted in nested object fields"

**Location**: `yaml_events.c` - `emit_event_for_kind()` when handling string scalars

```c
// yaml_events.c
static yaml_scalar_node_t* emit_scalar(const char *value) {
    // BUG: handles ASCII only, corrupts UTF-8
    // FIXED: use utf8_length() instead of strlen()
}
```

**Impact**:
- ✅ Context API users: Fixed (calls shared function)
- ✅ Convenience API users: Fixed (calls same shared function)
- ✅ No separate fix needed

### Scenario 2: Performance - Optimize Caching

**Improvement**: "Use faster cache lookup via hash table instead of linear search"

**Location**: `yaml_events.c` - `metac_yaml_serialization_measure()` caching logic

```c
// OLD:
for (i = 0; i < cached_count; i++) {
    if (equals(cached[i], current)) return cache[i];  // O(n)
}

// NEW:
return hash_map_lookup(cache, current);  // O(1)
```

**Impact**:
- ✅ Context API benefits: Faster measure() + to_buffer()
- ✅ Convenience API benefits: Faster metac_value_to_yaml_string()
- ✅ One optimization, both APIs faster
- ✅ Single implementation to review and test

### Scenario 3: Add Feature - Custom Tag Support

**Feature**: "Support custom YAML tags for specific types"

**Location**: Add parameter to context creation

```c
// Add to yaml_context.c
metac_yaml_serialization_t* metac_yaml_serialization_new(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    metac_tag_resolver_fn *p_resolver)  // NEW
{
    // ...
    serial->p_tag_resolver = p_resolver;
}

// Update yaml_events.c to use resolver
emit_scalar(value, serial->p_tag_resolver)  // Use new resolver

// Update api.c convenience wrapper
metac_value_to_yaml_string(..., metac_tag_resolver_fn *resolver)
{
    // ... pass resolver to new() ...
}
```

**Result**: Both APIs support new feature automatically!

---

## Testing Strategy

### Test Suite Organization

All tests in one file (`yaml_test.c`) exercising both APIs:

```c
START_TEST(test_yaml_context_api_simple_struct)
    - Tests direct context API calls
    - Exercises: new() → measure() → to_buffer()

START_TEST(test_yaml_single_pass_convenience)
    - Tests convenience wrapper
    - Exercises: metac_value_to_yaml_string()
    
START_TEST(test_yaml_context_reuse)
    - Tests caching benefit
    - Exercises: multiple to_buffer() calls on same context

START_TEST(test_yaml_unicode_nested)
    - Tests unicode in nested structures
    - Runs on BOTH APIs simultaneously
    - Ensures identical behavior
```

### Test Coverage

```
Core Functions (tested via both APIs):
  ✓ metac_yaml_serialization_new()      [tested by both]
  ✓ metac_yaml_serialization_measure()  [tested by both]
  ✓ metac_yaml_serialization_to_buffer()[tested by both]
  ✓ metac_yaml_serialization_delete()   [tested by both]

Wrapper Function:
  ✓ metac_value_to_yaml_string()        [specific tests]
  ✓ metac_yaml_free()                   [specific tests]

Result: When fixing test failure, fix applies to both APIs!
```

---

## Build Configuration

### Compilation

**Single compilation rule** (no separate targets):

```makefile
# src/serialization/yaml/Makefile (or main Makefile)

YAML_SRCS = \
    src/serialization/yaml/yaml_handler.c \
    src/serialization/yaml/yaml_context.c \
    src/serialization/yaml/yaml_events.c \
    src/serialization/yaml/api.c

# No separate rules needed - all compile together
libmetac_reflect += $(YAML_SRCS)
```

### Linking

**Single link target**:
```makefile
libmetac.a: $(libmetac_reflect_obj)
    $(AR) rcs $@ $^
    # Both APIs included, no conditional linking
```

### Header Export

**Single public header**:
```
include/metac/serialization/yaml.h
    ├─ metac_yaml_serialization_t [opaque type]
    ├─ metac_yaml_serialization_new()
    ├─ metac_yaml_serialization_measure()
    ├─ metac_yaml_serialization_to_buffer()
    ├─ metac_yaml_serialization_delete()
    ├─ metac_value_to_yaml_string()
    └─ metac_yaml_free()
```

---

## Design Principles Applied

1. **DRY (Don't Repeat Yourself)**
   - ~28 lines of wrapper code
   - ~250 lines of shared core
   - ~90% reuse achieved

2. **Single Responsibility**
   - Each file handles one concern:
     - `yaml_handler.c` → libyaml integration
     - `yaml_context.c` → lifecycle management
     - `yaml_events.c` → core YAML generation
     - `api.c` → user-facing convenience

3. **Separation of Concerns**
   - Public API layer (user-facing) ← thin
   - Implementation layer (shared core) ← thick
   - No mixing of concerns

4. **Testability**
   - Both APIs testable via single test suite
   - Core functions tested indirectly via both APIs
   - Bug fixes automatically test both code paths

---

## Summary: Why This Works

| Aspect | Benefit |
|--------|---------|
| **Code Reuse** | 90% shared, 10% glue code |
| **Maintainability** | Fix once, benefit both APIs |
| **Consistency** | Identical core, identical behavior |
| **Testing** | Single suite covers both APIs |
| **Compilation** | Single build target |
| **Feature Addition** | One implementation, two interfaces |
| **Performance** | No overhead from wrappers |

The key insight: **APIs are interfaces to the same implementation**, not separate implementations with separate interfaces.

This is how libraries like `cJSON`, `libyaml`, and standard C library work - multiple convenient entry points that internally use the same core algorithm.
