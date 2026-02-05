# Visual Architecture Summary

## The Question Answered

```
Question: "Will convenience function have separate code?"

Before:   ❌ SEPARATE IMPLEMENTATIONS
           Convenience  Context API
           Code         Code
           (duplicated) (duplicated)
           ↓            ↓
           Both serialize to YAML

After:    ✅ SHARED CORE ARCHITECTURE
           Convenience API    Context API
           (wrapper ~28 lines)  (direct)
                   ↓                 ↓
           Same Core Implementation
           (yaml_context.c, yaml_handler.c, yaml_events.c)
                   ↓
           Result: 90% code reuse
```

---

## Code Organization

```
┌──────────────────────────────────────────┐
│  src/serialization/yaml/                │
├──────────────────────────────────────────┤
│                                          │
│  SHARED (called by both APIs):           │
│  ├─ yaml_handler.c      [150 lines]     │
│  │  └─ libyaml write callback           │
│  │                                       │
│  ├─ yaml_context.c      [100 lines]     │
│  │  └─ context lifecycle                │
│  │                                       │
│  └─ yaml_events.c       [200+ lines]    │
│     ├─ measure()  ← key: caching        │
│     └─ to_buffer() ← uses cache         │
│                                          │
│  WRAPPER (calls shared):                 │
│  └─ api.c               [~30 lines]     │
│     ├─ new()            ← calls shared #1
│     ├─ measure()        ← calls shared #2
│     ├─ malloc()         ← NEW           │
│     ├─ to_buffer()      ← calls shared #3
│     └─ delete()         ← calls shared #4
│                                          │
│  TESTS (covers both):                    │
│  └─ yaml_test.c         [~100 lines]    │
│     ├─ test_context_api()               │
│     ├─ test_convenience_api()           │
│     └─ test_identical_output()          │
│                                          │
└──────────────────────────────────────────┘
```

---

## Execution Flow: Both APIs

```
Context API Direct Call          Convenience API Call
┌─────────────────────────┐     ┌─────────────────────────┐
│ metac_yaml_serialization│     │ metac_value_to_yaml_    │
│      _new()             │     │      string()           │
└──────────┬──────────────┘     └──────────┬──────────────┘
           │                                │
           │                               [wrapper]
           │                                │
           ↓                                ↓
    ┌──────────────┐              ┌──────────────┐
    │ new()        │←─────────────│ calls new()  │
    │ [shared]     │              │ [shared]     │
    └──────────────┘              └──────────────┘
           │                                │
           ↓                                ↓
    ┌──────────────┐              ┌──────────────┐
    │ measure()    │←─────────────│calls measure()
    │ [shared]     │              │ [shared]     │
    │ CACHES       │              │ CACHES       │
    │ iterator     │              │ iterator     │
    └──────────────┘              └──────────────┘
           │                                │
           ↓                                ↓
    ┌──────────────┐              ┌──────────────┐
    │ allocate     │              │malloc() buf  │
    │ (user's job) │              │[NEW in api.c]│
    └──────────────┘              └──────────────┘
           │                                │
           ↓                                ↓
    ┌──────────────┐              ┌──────────────┐
    │to_buffer()   │←─────────────│calls to_buffer()
    │[shared]      │              │ [shared]     │
    │uses cache    │              │uses cache    │
    │no re-walk    │              │no re-walk    │
    └──────────────┘              └──────────────┘
           │                                │
           ↓                                ↓
    ┌──────────────┐              ┌──────────────┐
    │delete()      │←─────────────│calls delete()
    │[shared]      │              │ [shared]     │
    └──────────────┘              └──────────────┘
           │                                │
           ↓                                ↓
      SUCCESS ✓                    SUCCESS ✓
```

**Result**: Same execution path, same behavior, single implementation!

---

## Maintenance Scenario Example

### Scenario: Fix Unicode Bug

```
BUG: "Chinese characters corrupted in nested arrays"

SHARED APPROACH:
┌──────────────────────────────────────────┐
│ yaml_events.c line 245                   │
│ Old: emit_scalar(utf8_str)               │
│      └─ uses strlen() (incorrect)        │
│ New: emit_scalar(utf8_str)               │
│      └─ uses utf8_length() (correct)     │
└──────────────────────────────────────────┘
       ↓
    Recompile
       ↓
   ┌─────────────────────────┬──────────────────────────┐
   ↓                         ↓                          ↓
Context API               Convenience API         All tests
FIXED ✓                   FIXED ✓                 PASS ✓
```

**Benefit**: Fix in one place fixes both APIs!

---

## Code Statistics

```
SHARED APPROACH (what we do):
┌─────────────────────────────────────┐
│ yaml_handler.c      150 lines       │
│ yaml_context.c      100 lines       │
│ yaml_events.c       200 lines       │
├─────────────────────────────────────┤
│ Subtotal (shared)   450 lines       │
│                                     │
│ api.c                30 lines       │
│ (wrapper)                           │
├─────────────────────────────────────┤
│ TOTAL              480 lines        │
│ Reuse ratio:        94%             │
└─────────────────────────────────────┘

DUPLICATE APPROACH (what NOT to do):
┌─────────────────────────────────────┐
│ Context API impl    450 lines       │
│ Convenience impl    450 lines       │
│ Test duplication    100 lines       │
├─────────────────────────────────────┤
│ TOTAL              1000 lines       │
│ Reuse ratio:         0%             │
│ Risk:               HIGH            │
└─────────────────────────────────────┘
```

---

## API Comparison Matrix

```
┌──────────────┬────────────────┬──────────────────┐
│  Aspect      │ Context API    │ Convenience API  │
├──────────────┼────────────────┼──────────────────┤
│ Entry calls  │ 4 (explicit)   │ 1 (implicit)     │
│              │ new()          │ to_yaml_string() │
│              │ measure()      │                  │
│              │ to_buffer()    │                  │
│              │ delete()       │                  │
│              │                │                  │
│ Memory model │ User allocates │ metac allocates  │
│              │ buffers        │ result           │
│              │                │                  │
│ Reusability  │ ✓ Yes (cache) │ ✗ No (one-shot) │
│              │                │                  │
│ Use case     │ High perf      │ Simple logging   │
│              │ Multiple out   │ Fire-and-forget  │
│              │ Resource       │                  │
│              │ constrained    │                  │
│              │                │                  │
│ Code path    │ Direct call    │ Wrapper call     │
│ to core      │ ↓ measure()    │ ↓ new()          │
│              │ ↓ to_buffer()  │ ↓ measure()      │
│              │                │ ↓ malloc()       │
│              │                │ ↓ to_buffer()    │
│              │                │ ↓ delete()       │
│              │                │                  │
│ Core code    │ SAME           │ SAME             │
│ identical    │ yaml_events.c  │ yaml_events.c    │
└──────────────┴────────────────┴──────────────────┘
```

---

## Testing Coverage

```
Single Test Suite (yaml_test.c):

Test 1: Context API
├─ new() → measure() → to_buffer() → delete()
├─ Verifies caching works
└─ Asserts correct YAML output

Test 2: Convenience API
├─ to_yaml_string() 
├─ Verifies internal orchestration
└─ Asserts correct YAML output

Test 3: Consistency
├─ Call both APIs with same input
├─ Compare outputs
└─ Assert identical results

Test 4: Cache Benefits
├─ Verify multiple to_buffer() calls use same cache
├─ Measure performance benefit
└─ Assert no re-traversal happens
```

**Result**: When a test fails, both code paths affected → catches issues early!

---

## Conclusion: Architecture Benefits

```
┌─────────────────────────────────────┐
│ Shared Code Architecture = Wins     │
├─────────────────────────────────────┤
│                                     │
│ For Developers:                     │
│ ✓ Clear design (one core path)      │
│ ✓ Easy debugging (shared code)      │
│ ✓ No duplicate bugs                 │
│                                     │
│ For Users:                          │
│ ✓ Two API choices                   │
│ ✓ Identical results                 │
│ ✓ No performance penalty            │
│                                     │
│ For Maintainers:                    │
│ ✓ Fix once, benefit both            │
│ ✓ Feature once, benefit both        │
│ ✓ Test once, cover both             │
│ ✓ Single source of truth            │
│                                     │
└─────────────────────────────────────┘
```

---

## See Also

- [SHARED_CODE_ARCHITECTURE.md](SHARED_CODE_ARCHITECTURE.md) - Full architecture document
- [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) - Implementation details
- [YAML_API_REFERENCE_CARD.md](YAML_API_REFERENCE_CARD.md) - Quick reference
- [SHARED_CODE_COMPLETION.md](SHARED_CODE_COMPLETION.md) - Completion summary
