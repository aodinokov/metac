# YAML API Quick Reference Card

## Two APIs, One Core

```
Context API                        Convenience API
(explicit, reusable)               (implicit, simple)

serial = new(val, mode, tags)      yaml = to_yaml_string(val, mode, tags, &sz)
measure(serial, &sz)               if (yaml) {
allocate(buf, sz)                      use(yaml)
to_buffer(serial, buf, sz)            free(yaml)
to_buffer(serial, buf2, sz)        }
delete(serial)
```

Both use same core: `measure()` and `to_buffer()` functions

---

## Core Functions (Shared Code)

```c
// Context creation
metac_yaml_serialization_t* new(value, mode, tags)
    → Create context, NO traversal yet
    → Returns opaque handle

// Measure + Cache (KEY: caching happens here)
int measure(serial, &size)
    → Full traversal: create iterator, generate events
    → CACHE iterator state ← reusable
    → Return measured size

// Serialize using cache
int to_buffer(serial, buf, bufsize, &written)
    → Use cached iterator (no re-traversal!)
    → Write events to user buffer
    → Can be called multiple times

// Cleanup
void delete(serial)
    → Free cached iterator
    → Free context
```

---

## When to Use Each API

### Context API: Use When...
- ✓ Need to serialize to multiple outputs
- ✓ Performance critical (large nested structures)
- ✓ Want full control over allocations
- ✓ Memory constraints matter

### Convenience API: Use When...
- ✓ One-off serialization
- ✓ Logging/debugging output
- ✓ Code simplicity matters more
- ✓ Malloc overhead acceptable

---

## Code Examples

### Context API (Reusable)
```c
// Measure once
metac_yaml_serialization_t *s = metac_yaml_serialization_new(pv, mode, NULL);
size_t sz;
metac_yaml_serialization_measure(s, &sz);
char *buf = malloc(sz);

// Serialize to multiple places (reuse cache!)
metac_yaml_serialization_to_buffer(s, buf, sz, NULL);
fwrite(buf, 1, sz, stdout);

metac_yaml_serialization_to_buffer(s, buf, sz, NULL);  // Cached!
write_to_network(buf, sz);

free(buf);
metac_yaml_serialization_delete(s);
```

### Convenience API (Simple)
```c
size_t sz;
char *yaml = metac_value_to_yaml_string(pv, mode, NULL, &sz);
if (yaml) {
    puts(yaml);
    metac_yaml_free(yaml);
}
```

---

## Architecture: Shared Core

```
┌──────────────────┐         ┌──────────────┐
│  Context API     │         │ Convenience  │
│  (explicit)      │         │   (simple)   │
└────────┬─────────┘         └────┬─────────┘
         │                        │
         │ calls:                 │ calls:
         │ new()                  │ internally:
         │ measure()              │ new()
         │ to_buffer()            │ measure()
         │ delete()               │ malloc
         │                        │ to_buffer()
         │                        │ delete()
         ↓                        ↓
      ┌──────────────────────────────┐
      │   SHARED CORE (250 lines)    │
      │                              │
      │   yaml_context.c             │
      │   yaml_handler.c             │
      │   yaml_events.c              │
      │   (measure, to_buffer)       │
      └──────────────────────────────┘
```

**Key**: Same core function, different entry points

---

## Error Handling

```c
int result = metac_yaml_serialization_to_buffer(serial, buf, sz, &written);

if (result == 0) {
    // Success: buf[0..written-1] contains YAML
}
else if (result == -ENOSPC) {
    // Buffer too small: written = required size
}
else if (result == -EINVAL) {
    // Invalid arguments
}
else if (result == -EIO) {
    // libyaml internal error
}
```

---

## Common Patterns

### Pattern 1: Measure, Allocate, Serialize
```c
size_t sz;
metac_yaml_serialization_measure(serial, &sz);
char *buf = malloc(sz);
metac_yaml_serialization_to_buffer(serial, buf, sz, NULL);
```

### Pattern 2: Multiple Destinations (Reuse Cache)
```c
// Serialize to file
FILE *f = fopen("data.yaml", "w");
metac_yaml_serialization_to_buffer(serial, buf, sz, NULL);
fwrite(buf, 1, sz, f);
fclose(f);

// Serialize to network (reuses cached iterator!)
metac_yaml_serialization_to_buffer(serial, buf, sz, NULL);
send_data(buf, sz);
```

### Pattern 3: Stack Buffer with Fallback
```c
char stack_buf[1024];
size_t sz;
metac_yaml_serialization_measure(serial, &sz);

if (sz <= sizeof(stack_buf)) {
    metac_yaml_serialization_to_buffer(serial, stack_buf, sizeof(stack_buf), NULL);
} else {
    char *heap_buf = malloc(sz);
    metac_yaml_serialization_to_buffer(serial, heap_buf, sz, NULL);
    free(heap_buf);
}
```

### Pattern 4: Fire-and-Forget
```c
size_t sz;
char *yaml = metac_value_to_yaml_string(pv, METAC_WMODE_deep, NULL, &sz);
if (yaml) {
    // Use yaml
    metac_yaml_free(yaml);
}
```

---

## Implementation Details

| Aspect | Details |
|--------|---------|
| **Traversal passes** | 2 (measure + serialize, optimized) |
| **Internal allocations** | 1 iterator allocation in measure() |
| **Iterator caching** | Yes - reusable in to_buffer() |
| **Malloc in core** | No (user provides buffer) |
| **Malloc in convenience** | 1 allocation for result string |
| **Code reuse** | 90% (250 shared / 280 total) |
| **Performance overhead** | None - wrapper is ~30 lines |

---

## File Organization

```
src/serialization/yaml/
├── yaml_handler.c    ← Shared: libyaml callback
├── yaml_context.c    ← Shared: lifecycle
├── yaml_events.c     ← Shared: core algorithm
├── api.c             ← Wrapper: convenience only
└── yaml_test.c       ← Tests: both APIs
```

Single `api.c` file (30 lines) for ALL convenience functions!

---

## Maintenance

**Bug fix**: Patch core once, both APIs fixed
```
Bug in yaml_events.c
    ↓
Fix measure() and/or to_buffer()
    ↓
Both Context API and Convenience API fixed ✓
```

**Feature addition**: One implementation, two interfaces
```
Add tag customization:
    ↓
Modify yaml_events.c + context
    ↓
Context API gets feature + Convenience API gets feature ✓
```

---

## Summary Table

| Feature | Context | Convenience |
|---------|---------|-------------|
| Reusable | ✓ Yes | ✗ No |
| Memory control | ✓ Full | ✗ Internal |
| Multiple outputs | ✓ Easy | ✗ Requires new call |
| Code simplicity | ✗ More verbose | ✓ Simple |
| Performance | ✓ Optimal | ✓ Same |
| Cache benefit | ✓ Explicit | ✓ Automatic |
| Implementation | ✓ Direct | ✓ Shared wrapper |

---

## Reference Links

- [SHARED_CODE_ARCHITECTURE.md](SHARED_CODE_ARCHITECTURE.md) - Architecture overview
- [YAML_QUICK_START.md](YAML_QUICK_START.md) - Full API reference
- [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) - Implementation structure
- [README.md](README.md) - All documentation links
