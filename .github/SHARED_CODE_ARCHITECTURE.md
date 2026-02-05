# YAML API Design: Two APIs, Shared Core (Architecture Summary)

## The Question

> "Теперь с наличием конвиниенс функции - вопрос такой - ее имплементация будет исползовать отдельный код (совсем отдельная ветка?) или будет много общего кода с 2х проходным api?"

**Translation**: "Now with the convenience function - the question is: will its implementation use separate code (a completely different branch?) or will there be a lot of shared code with the 2-pass API?"

## The Answer

**Shared code approach** - the convenience function is a thin wrapper (~28 lines) around the core context API (~250 lines).

```
Convenience function is NOT:  ❌ Separate implementation
Convenience function IS:      ✅ Wrapper around shared core
```

---

## Visual Architecture

### Single Implementation, Multiple Entry Points

```
┌────────────────────────────────────────────────────────┐
│  User Code                                             │
├────────────────────────────────────────────────────────┤
│                                                        │
│  Option A:                    Option B:               │
│  ┌──────────────────┐        ┌──────────────────┐    │
│  │ Context API      │        │ Convenience API  │    │
│  │ (explicit)       │        │ (implicit)       │    │
│  │                  │        │                  │    │
│  │ new()            │        │ to_yaml_string() │    │
│  │ measure()        │        │     ↓            │    │
│  │ to_buffer() x N  │        │ (calls A:        │    │
│  │ delete()         │        │  new,measure,    │    │
│  └────────┬─────────┘        │  to_buffer,      │    │
│           │                  │  delete)         │    │
│           │                  └────────┬─────────┘    │
│           │                           │              │
│           └───────────────┬───────────┘              │
│                           │                          │
└───────────────────────────┼──────────────────────────┘
                            ↓
┌────────────────────────────────────────────────────────┐
│  Shared Core Implementation                            │
├────────────────────────────────────────────────────────┤
│                                                        │
│  yaml_context.c (100 lines)                           │
│  ├─ Context allocation/deallocation                  │
│  └─ Emitter initialization                           │
│                                                        │
│  yaml_handler.c (150 lines)                           │
│  └─ libyaml write callback                           │
│                                                        │
│  yaml_events.c (200+ lines) ← Core logic              │
│  ├─ measure() - full traversal + cache               │
│  ├─ to_buffer() - use cached data                    │
│  └─ Event generation helpers                          │
│                                                        │
│  Total: ~250 lines of shared implementation           │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Code Organization

```
src/serialization/yaml/
├── yaml_handler.c      ← Shared: libyaml integration
├── yaml_context.c      ← Shared: context lifecycle
├── yaml_events.c       ← Shared: core serialization logic
├── api.c               ← Wrapper: calls 1-4 functions from shared code
└── yaml_test.c         ← Tests: covers both APIs with same test suite
```

---

## Implementation Example

### What the Wrapper Does

```c
// api.c - convenience function (28 lines total)
char* metac_value_to_yaml_string(
    metac_value_t *p_val,
    metac_value_walk_mode_t wmode,
    metac_tag_map_t *p_tag_map,
    size_t *p_out_size) {
    
    // Step 1: Call shared core function #1
    metac_yaml_serialization_t *serial = 
        metac_yaml_serialization_new(p_val, wmode, p_tag_map);
    
    // Step 2: Call shared core function #2
    size_t required;
    metac_yaml_serialization_measure(serial, &required);
    
    // Step 3: New code - allocate buffer (only malloc-specific part)
    char *buf = malloc(required + 1);
    
    // Step 4: Call shared core function #3
    metac_yaml_serialization_to_buffer(serial, buf, required, NULL);
    
    // Step 5: Call shared core function #4
    metac_yaml_serialization_delete(serial);
    
    // Done - return
    buf[required] = '\0';
    *p_out_size = required + 1;
    return buf;
}
```

**Key point**: Only 1 line is truly "new" (malloc). Everything else calls shared functions.

---

## Code Statistics

| Component | Lines | Purpose |
|-----------|-------|---------|
| **yaml_handler.c** | 150 | Shared: libyaml callback |
| **yaml_context.c** | 100 | Shared: context lifecycle |
| **yaml_events.c** | 200+ | Shared: core algorithm |
| **api.c** | 30 | Wrapper: convenience only |
| **Total** | ~480 | Both APIs fully supported |

### Comparison: Shared vs Duplicate

```
SHARED APPROACH (what we do):
├─ Shared core: 450 lines
└─ Wrapper: 30 lines
  Total: 480 lines → 90% reuse

DUPLICATE APPROACH (what NOT to do):
├─ Context implementation: 450 lines
├─ Convenience implementation: 450 lines (copy)
└─ Test duplication: 100 lines
  Total: 1000 lines → 0% reuse, maintenance nightmare
```

---

## Maintenance Benefits

### Scenario 1: Bug Fix

```
BUG: "Unicode corrupted in nested structs"

SHARED APPROACH:
  1. Fix yaml_events.c emit_scalar()
  2. Recompile
  3. Both APIs fixed ✓

DUPLICATE APPROACH:
  1. Fix yaml_events_context.c emit_scalar()
  2. Fix yaml_events_convenience.c emit_scalar()  ← Easy to forget
  3. Recompile
  4. Risk: Inconsistent fixes
```

### Scenario 2: Performance Optimization

```
IMPROVEMENT: "Cache lookups 2x faster"

SHARED APPROACH:
  1. Optimize yaml_events.c caching
  2. Recompile
  3. Both APIs 2x faster ✓

DUPLICATE APPROACH:
  1. Optimize yaml_events_context.c
  2. Optimize yaml_events_convenience.c ← Easy to forget
  3. Risk: One API faster than other
```

### Scenario 3: New Feature

```
FEATURE: "Support custom YAML tags"

SHARED APPROACH:
  1. Add p_tag_resolver to context
  2. Update measure() and to_buffer()
  3. Update convenience wrapper
  4. Both APIs support feature ✓

DUPLICATE APPROACH:
  1. Add p_tag_resolver to context v1
  2. Add p_tag_resolver to context v2 ← Easy to miss
  3. Risk: Inconsistent feature support
```

---

## Testing Strategy

### Single Test Suite Covers Both

```c
// yaml_test.c

// Test 1: Context API
START_TEST(test_yaml_context_api) {
    serial = metac_yaml_serialization_new(pv, ...);
    metac_yaml_serialization_measure(serial, &sz);
    metac_yaml_serialization_to_buffer(serial, buf, sz, NULL);
    metac_yaml_serialization_delete(serial);
    assert(yaml_is_valid(buf));
}

// Test 2: Convenience API
START_TEST(test_yaml_convenience_api) {
    yaml = metac_value_to_yaml_string(pv, ..., &sz);
    assert(yaml_is_valid(yaml));
    metac_yaml_free(yaml);
}

// Test 3: Identical output
START_TEST(test_yaml_apis_identical) {
    yaml_context = serialize_via_context(pv);
    yaml_convenience = serialize_via_convenience(pv);
    assert_strings_equal(yaml_context, yaml_convenience);
}
```

**Result**: When you fix a test failure, the fix applies to both code paths!

---

## Build Integration

### Single Compilation

```makefile
# Makefile
YAML_SRCS = \
    src/serialization/yaml/yaml_handler.c \
    src/serialization/yaml/yaml_context.c \
    src/serialization/yaml/yaml_events.c \
    src/serialization/yaml/api.c

libmetac += $(YAML_SRCS)
```

**No separate compilation targets**:
- No `libmetac_context_only`
- No `libmetac_with_convenience`
- Just one library with both APIs

---

## Architecture Principles

### 1. Single Responsibility
Each file does ONE thing:
- `yaml_handler.c` → libyaml integration
- `yaml_context.c` → context lifecycle
- `yaml_events.c` → YAML generation (core)
- `api.c` → convenience wrapper

### 2. Don't Repeat Yourself (DRY)
Core logic exists ONCE. Both APIs use it.

### 3. Separation of Concerns
- Core algorithm in one place
- Wrappers are thin and obvious
- Easy to review and maintain

### 4. User Choice
Two interfaces to same implementation:
- Context API: "I want control"
- Convenience API: "I want simple"

---

## Comparison: Different Architectures

### Architecture 1: Shared Core ✅ (What we do)

```
API1 ──┐     ┌──→ Event generation
       ├──→  ├──→ Iterator caching
API2 ──┘     └──→ Buffer management

Lines: API1=5, API2=28, Core=250 → 283 total
Maintenance: Single core fixes both
Consistency: Guaranteed identical behavior
```

### Architecture 2: Duplicate Code ❌ (Anti-pattern)

```
API1 ──→ Event generation v1
         Iterator caching v1
         Buffer management v1

API2 ──→ Event generation v2
         Iterator caching v2
         Buffer management v2

Lines: API1=255, API2=255, Total=510
Maintenance: Fix both independently (easy to miss)
Consistency: Risk of divergence
```

### Architecture 3: Separate Branches ❌ (Anti-pattern)

```
if (use_context_api) {
    context_api_serialization();
} else {
    convenience_api_serialization();  ← Completely different code
}

Maintenance: Code reviews must understand both paths
Testing: Two separate code paths to test
Consistency: Divergence over time inevitable
```

---

## Why This Matters

### For Developers
- **Clear code**: Core logic is obvious
- **Easy to fix**: Bug fix in one place
- **Confidence**: Tests run on both paths

### For Users
- **Two choices**: Pick what fits your use case
- **Identical behavior**: Guaranteed results
- **No performance hit**: Wrapper has zero overhead

### For Maintainers
- **Single source of truth**: Core algorithm in one place
- **Reduced bugs**: Fix half the code, fix all functionality
- **Faster feature addition**: Add once, available everywhere

---

## Summary

| Aspect | Single Core (✅) | Duplicate (❌) |
|--------|---|---|
| **Code size** | 283 lines | 510 lines |
| **Reuse ratio** | 90% | 0% |
| **Fix locations** | 1 | 2+ |
| **Test paths** | 1 core + 2 interfaces | 2 separate paths |
| **Maintenance** | Easy ✓ | Hard ✗ |
| **Consistency** | Guaranteed | Risk |
| **Performance** | No overhead | No overhead |
| **Understandability** | Clear | Complex |

---

## Conclusion

The convenience function uses the **exact same implementation** as the context API. It's not a separate code branch - it's a thin orchestration wrapper.

This approach:
- ✅ Minimizes code duplication
- ✅ Simplifies maintenance
- ✅ Guarantees consistency
- ✅ Reduces bugs
- ✅ Makes future changes easier

**Both APIs share:**
- Same core algorithm
- Same performance
- Same behavior
- Same test coverage

---

## See Also

- [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) - Implementation file structure and flow
- [YAML_API_DESIGN_RATIONALE.md](YAML_API_DESIGN_RATIONALE.md) - Why two APIs?
- [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md) - Code implementation templates
- [YAML_QUICK_START.md](YAML_QUICK_START.md) - User-facing API reference
