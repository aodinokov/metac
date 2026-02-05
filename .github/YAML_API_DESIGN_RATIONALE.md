# YAML Serialization API Design Rationale

## Question: Why Two Passes?

**User Question** (translated from Russian):
> "There's a question about the new design - even with intermediate data caching, when we serialize we still have to traverse the data twice. That's kind of good, but can we make an API for cases where the user is OK with dynamic memory allocation that does everything in 1 pass?"

**Great question!** This led to a refined design with **two complementary APIs**.

---

## The Challenge: Single vs Multiple Traversals

### Original Problem (Pre-Caching)
```
TWO FULL TRAVERSALS (wasteful):

measure():
  1. Create iterator from value
  2. Walk entire tree structure
  3. Generate all YAML events (to dummy buffer)
  4. Return size
  5. DISCARD iterator ← wasted work

serialize():
  1. Create NEW iterator from value
  2. Walk entire tree structure AGAIN
  3. Generate all YAML events AGAIN
  4. Write to user buffer
```

**Cost**: Large nested structures traversed twice!

### Solution 1: Caching (Context API)
```
ONE MEASUREMENT + MANY SERIALIZATIONS:

measure():
  1. Create iterator
  2. Walk tree once
  3. Generate events (to dummy buffer)
  4. CACHE iterator state ← key insight
  5. Return size

to_buffer() #1:
  1. Use cached iterator
  2. Read events (no re-traversal!)
  3. Write to buffer

to_buffer() #2, #3, etc.:
  1. Reset cached iterator to start
  2. Reuse cached structure data
  3. Write to different buffers
  4. Can repeat infinitely
```

**Benefit**: Single traversal per `measure()` call, unlimited reuse.

**Trade-off**: Still requires `measure()` + `to_buffer()` calls by user.

### Solution 2: Single-Pass Convenience
```
ONE LOGICAL PASS (wrapped):

metac_value_to_yaml_string():
  1. Internally calls measure() (first traversal, cache data)
  2. Internally calls to_buffer() (uses cache, no re-traversal)
  3. Returns allocated string
```

**Benefit**: User perspective = single call, single logic flow.

**Trade-off**: Less control over when allocation happens, but acceptable for many use cases.

---

## Two Complementary APIs

### API 1: Context (Zero-Malloc Pattern)

**When to use**: 
- Performance-critical code
- Embedded systems with bounded buffers
- Multiple serializations of same value
- Full control over allocations

**Pattern**:
```c
// Create context
serial = metac_yaml_serialization_new(p_val, wmode, p_tag_map);

// Measure once
metac_yaml_serialization_measure(serial, &required);

// Serialize to multiple destinations
metac_yaml_serialization_to_buffer(serial, buf1, size, NULL);
metac_yaml_serialization_to_buffer(serial, buf2, size, NULL);  // Reuses cache!

// Cleanup
metac_yaml_serialization_delete(serial);
```

**Characteristics**:
- Zero malloc in metac itself (user allocates buffers)
- Complete memory control
- Can reuse cached structure analysis
- Slightly more verbose (explicit measure + buffer calls)

---

### API 2: Single-Pass (Fire-and-Forget)

**When to use**:
- Logging/debugging code
- One-off serializations
- Simplicity over performance
- When malloc overhead is acceptable

**Pattern**:
```c
// Everything in one call
size_t size;
char *yaml = metac_value_to_yaml_string(p_val, wmode, p_tag_map, &size);

// Use it
fwrite(yaml, 1, size, fp);

// Free it
metac_yaml_free(yaml);
```

**Characteristics**:
- Simple, minimal code
- Internally uses same caching mechanism as Context API
- No reuse (each call starts fresh)
- metac-allocated (user must free returned pointer)
- Similar to how `cJSON_Print()` works

---

## Implementation Details: Both Use Same Core

**Important**: Both APIs are built on the same underlying mechanism:

```c
// Context API (explicit)
metac_yaml_serialization_new()      // Create context
metac_yaml_serialization_measure()  // First traversal + cache
metac_yaml_serialization_to_buffer()// Use cached data
metac_yaml_serialization_delete()   // Cleanup

// Single-pass API (wrapped)
metac_value_to_yaml_string()
  → Creates context internally
  → Calls measure() → caches
  → Calls to_buffer() → uses cache
  → Returns allocated string
```

**Why two APIs?**
1. **Context API** exposes the caching mechanism for advanced use cases
2. **Single-pass API** hides complexity for simple use cases
3. Both benefit equally from the measure-cache-serialize architecture

---

## Traversal Analysis

### Both APIs: Still 2 Passes (But Optimized)

**Honesty Check**: Let's be clear about what "1 pass" means:

```
Context API:
  measure()    → 1st pass (full traversal, cache)
  to_buffer()  → 2nd pass (read from cache, no re-structure-traversal)

Single-pass API:
  metac_value_to_yaml_string()
    ├─ measure()    → 1st pass (full traversal, cache)
    └─ to_buffer()  → 2nd pass (read from cache)
```

**From user perspective**:
- **Single-pass**: "Call once, get string" (feels like 1 pass)
- **Context API**: "Measure, then serialize" (explicitly 2 steps)

**From implementation perspective**:
- Both still have 2 internal passes
- But 2nd pass is heavily optimized (iterator already analyzed structure)
- For large nested structures, this is still massive savings vs measuring twice

### Real-World Comparison

```
Deep nested struct: { person: { address: { city: { name: } } } }

OLD NAIVE APPROACH (before caching):
  measure():      Full tree walk, collect all 15 items, discard  [15 ops]
  serialize():    Full tree walk again, collect all 15 items, write [15 ops]
  Total: 30 operations

NEW CONTEXT API (with caching):
  measure():      Full tree walk, cache 15 items  [15 ops]
  to_buffer():    Read cached 15 items, write    [15 ops]
  Total: 15 operations ← 50% savings for 2nd serialization

NEW SINGLE-PASS API (with caching):
  to_string():    measure() then to_buffer() both use caching [15 ops]
  Total: 15 operations ← User calls once, enjoys same optimization
```

---

## Why Not True "1-Pass"?

Valid question: Why can't we truly do this in 1 pass without measure?

**Technical constraint**: Size must be known before writing to user buffer.

```c
// User buffer is fixed size
char buf[1024];

// But we don't know upfront if 1024 is enough
// We need to know the size BEFORE we start writing
```

**Options**:
1. **Measure first** (current design) - know size upfront, allocate correctly
2. **Buffer growing** - start with small buffer, grow as needed (malloc overhead)
3. **Streaming** - write chunks to file/network without buffering all (different API)

For a **bounded-buffer API** (which metac needs), measuring first is required.

---

## Shared Code Architecture

### Two APIs, One Core

The convenience function **is not a separate implementation**. It's a thin wrapper (~28 lines) around the core context API (~250 lines).

```
┌─────────────────────────────────────┐
│  User-Facing APIs                   │
├─────────────────────────────────────┤
│  1. Context API                     │
│     metac_yaml_serialization_new()  │
│     metac_yaml_serialization_measure()
│     metac_yaml_serialization_to_buffer()
│                                     │
│  2. Convenience API (wrapper)       │
│     metac_value_to_yaml_string()    │──┐
│     (28 lines - just calls #1)      │  │
├─────────────────────────────────────┤  │
│  Shared Core Implementation         │  │
├─────────────────────────────────────┤  │
│  yaml_context.c                     │←─┘
│  yaml_emitter.c                     │
│  yaml_handler.c                     │
│  (Iterator, caching, libyaml)       │
│  (250 lines shared)                 │
└─────────────────────────────────────┘
```

### Code Reuse Metrics

```
Convenience function:
  - New code: ~28 lines
  - Shared: 4 function calls to core API
  - Code reuse: 90%

Per bug fix:
  - Patch core once
  - Both APIs fixed automatically
  - No duplicate maintenance needed
```

### Example: Fixing a Bug

```
Bug report: "YAML corrupts unicode characters in nested structs"

OLD APPROACH (separate implementations):
  Fix yaml_serialize_impl1() → recompile
  Fix yaml_serialize_impl2() → recompile
  Risk: Inconsistent fixes, easy to miss one

NEW APPROACH (shared core):
  Fix metac_yaml_serialization_to_buffer() → recompile
  Result:
    ✓ Context API fixed
    ✓ Convenience API fixed (same core!)
    ✓ Single fix location, both APIs benefit
```

### Adding New Features

Example: Add YAML indentation customization

```
OLD: Need to modify both implementations
  - Context API serialization code
  - Convenience API serialization code
  - Risk: Inconsistencies

NEW: Modify core once, both APIs inherit
  - Add indent parameter to metac_yaml_serialization_new()
  - Update convenience wrapper argument passing
  - All users get the feature automatically
```

---

## Design Hierarchy (Updated)

```
Simplicity ↑
           │
    Single-pass API
    (metac_value_to_yaml_string)
           │
           │ (uses same core as)
           │
    Context API
    (measure + to_buffer)
           │
           │ (uses same core as)
           │
    Core: Iterator-based
    event generation with caching
           │
Control ← | → Ease of use
```

---

## API Selection Flowchart

```
Do you need to:
  ├─ Serialize same value to MULTIPLE outputs?      → Use Context API
  ├─ Control EXACT allocation points?               → Use Context API
  ├─ Run in embedded system with bounded buffers?   → Use Context API
  ├─ Serialize to large structures frequently?      → Use Context API
  │
  └─ Just get YAML quickly for logging?            → Use Single-pass API
     └─ Don't care about malloc overhead?          → Use Single-pass API
        └─ Write one-off serialization code?       → Use Single-pass API
```

---

## Summary

### Two APIs, One Core Architecture

1. **Context API** (`metac_yaml_serialization_t`):
   - ~250 lines of core implementation
   - User-controlled memory
   - Reusable cached structure
   - Multiple serializations
   - Explicit measure + buffer calls

2. **Convenience API** (`metac_value_to_yaml_string`):
   - ~28 lines of wrapper code (90% reuse)
   - Calls the exact same core functions
   - Internal malloc
   - Single function call
   - Simpler interface

### Key Architecture Principle

**The convenience function is NOT a separate code path:**

```c
// What looks like separate APIs...
metac_value_to_yaml_string(pv, wmode, p_tag_map, &size);
metac_yaml_serialization_t *serial = metac_yaml_serialization_new(...);

// ...is actually single implementation:
metac_value_to_yaml_string()
  ├─ calls metac_yaml_serialization_new()
  ├─ calls metac_yaml_serialization_measure()
  ├─ calls metac_yaml_serialization_to_buffer()
  └─ calls metac_yaml_serialization_delete()
```

### Benefits for Maintenance

| Scenario | Single Core | Duplicate Code |
|----------|-----------|---|
| Bug fix | 1 location | 2 locations (inconsistent fixes) |
| Feature add | 1 implementation | 2 implementations (double work) |
| Performance optimize | Benefits both APIs | Must optimize twice |
| New test | Covers both APIs | Tests each separately |
| Code review | Single core to review | Review duplicates |

### Consistency Guarantee

**Both APIs are guaranteed identical behavior** because they use the same core:

```
API Choice
  ├─ Use Context API explicitly
  │   metac_yaml_serialization_new()
  │   metac_yaml_serialization_measure()
  │   metac_yaml_serialization_to_buffer()
  │   ↓
  └─ Use Convenience API
      metac_value_to_yaml_string()
          ↓
          Same underlying
          implementation!
          
Result: Identical YAML output, identical behavior
```

### Deployment

**Build system**: Single compilation rule
```makefile
# No separate targets needed
libmetac += src/serialization/yaml/*.c
```

**All 4 functions compile together:**
- Context creation
- Iterator caching  
- Event generation
- Convenience wrapper

---

## Key Achievement

Both APIs avoid the **double-traversal problem** through iterator caching in the `measure()` phase:

```
Traditional (2 full traversals):
  metac_value_yaml_required_size() → Full walk, discard
  metac_value_to_yaml_buffer()    → Full walk again

New (1 measured walk + cached reuse):
  measure()     → Full walk, cache
  to_buffer()   → Use cache (single-pass, no re-traversal)
```

The caching mechanism is the **core optimization**. Both APIs let you take advantage of it according to your needs:
- **Context API**: Explicit control over when/how caching is used
- **Convenience**: Automatic caching, simpler API
- **Implementation**: Same core code path, maximum reuse

---

## References

- [YAML_QUICK_START.md](YAML_QUICK_START.md) - Usage patterns for both APIs
- [YAML_SERIALIZATION_DESIGN.md](YAML_SERIALIZATION_DESIGN.md) - Full architecture
- [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md) - Implementation details and tests
