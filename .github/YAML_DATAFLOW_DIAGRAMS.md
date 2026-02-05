# YAML Serialization - Data Flow Diagram

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    User Application                             │
│  struct { int x; char *name; } my_data;                         │
│  metac_value_t *pv = METAC_VALUE_FROM_DECLLOC(loc, my_data);   │
└────────────────────────────┬──────────────────────────────────────┘
                             │
                             │ pv (metac_value_t*)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│            metac_value_to_yaml_buffer()                         │
│  - Takes user buffer, size, walk_mode, tag_map                  │
│  - Creates metac_yaml_context_t                                 │
└────────────────────────────┬──────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│        metac_yaml_emitter_setup()                               │
│  - yaml_emitter_initialize()                                    │
│  - yaml_emitter_set_output(write_handler, context)              │
│                                                                  │
│    KEY: write_handler knows user buffer location & size         │
└────────────────────────────┬──────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│        metac_value_to_yaml_impl()                               │
│                                                                  │
│  Create iterator: p_iter = metac_new_recursive_iterator(pv)    │
│                                                                  │
│  LOOP: for each value from iterator:                            │
│    ├─ Start state: examine kind (struct/array/scalar)           │
│    │  ├─ If struct:  emit MAPPING_START + create field deps   │
│    │  ├─ If array:   emit SEQUENCE_START + create element deps│
│    │  └─ If scalar:  emit SCALAR                               │
│    │                                                             │
│    └─ Finish state: emit MAPPING_END / SEQUENCE_END            │
└────────────────────────────┬──────────────────────────────────────┘
                             │
                   ▼─────────┴─────────▼
        ┌───────────────────────────────────────────┐
        │   yaml_emitter_emit(&event)               │
        │   (libyaml generates YAML text)           │
        └──────────────────┬────────────────────────┘
                           │
                           │ YAML chunks (10-100 bytes)
                           ▼
        ┌──────────────────────────────────────────┐
        │  metac_yaml_write_handler()               │
        │  ┌──────────────────────────────────────┐ │
        │  │ Check: current_pos + size <= buf_sz? │ │
        │  │    Yes: memcpy(buffer+pos, data, sz) │ │
        │  │    No:  overflow = 1; return FAIL    │ │
        │  └──────────────────────────────────────┘ │
        └────────────────────┬─────────────────────┘
                             │
                             ▼
        ┌──────────────────────────────────────────┐
        │    User Buffer (malloc'd or stack)       │
        │    ┌──────────────────────────────────┐  │
        │    │ "x: 10                           │  │
        │    │ name: myname                     │  │
        │    │ ..." (growing)                   │  │
        │    └──────────────────────────────────┘  │
        │    ^pos                        ^buf_size │
        │    └────────────────────────────┘        │
        └──────────────────────────────────────────┘
                             │
                             │ p_bytes_written = pos
                             ▼
        ┌──────────────────────────────────────────┐
        │  Return: error code + size written       │
        │  buffer now contains valid YAML          │
        │  (caller owns it - can write to file,    │
        │   network, etc.)                         │
        └──────────────────────────────────────────┘
```

## Iterator State Transitions

```
Entry: metac_value_t (root value)
                │
                ▼
    ┌─────────────────────────┐
    │ Iterator.next()         │ ← Get first task
    │ Returns: root value     │
    │ State: METAC_R_ITER_start│
    └────────────┬────────────┘
                 │
         ┌───────▼─────────┐
         │ Is it struct?   │
         └───┬─────────┬───┘
             │YES      │NO
             ▼         │
      ┌───────────┐    │
      │EMIT       │    │
      │MAPPING_   │    │
      │START      │    │
      │           │    │
      │for each   │    │
      │field:     │    │
      │create_dep │    │
      │set_state 1│    │
      └─────┬─────┘    │
            │          │
    ┌───────▼──────────▼──────────┐
    │ Iterator.next()             │ ← Get next unfinished task
    │ Returns: first field task   │
    │ State: METAC_R_ITER_start   │
    └────────────┬────────────────┘
                 │
         ┌───────▼──────────┐
         │ Is it scalar?    │
         └───┬──────────┬───┘
             │YES       │NO (struct/array)
             ▼          │
        ┌───────────┐   │
        │ EMIT      │   │ ←recurs
        │ SCALAR    │   │  back to
        │           │   │ struct/
        │done()     │   │ array
        └─────┬─────┘   │ logic
              │         │
              └─────┬───┘
                    │
            ┌───────▼──────────┐
            │Iterator.next()   │ ← Get next sibling/parent
            │Back to field #2? │
            │Or struct done?   │
            └─────┬──────┬─────┘
                  │      │
          struct? │      │ no more
                  ▼      ▼
           (loop) done   ┌─────────────────┐
                         │ Iterator.next() │
                         │ Returns: NULL   │
                         │ (end)           │
                         └─────────────────┘
                                 │
                                 ▼
                         ┌─────────────────┐
                         │EMIT             │
                         │DOCUMENT_END     │
                         │STREAM_END       │
                         │                 │
                         │return 0         │
                         └─────────────────┘
```

## Memory Flow Diagram

```
┌──────────────────────────────────────────────────────────────┐
│  User Code                                                   │
│  char buf[4096];  ◄─────────────────┐                        │
│  metac_value_to_yaml_buffer(pv,    │                        │
│    buf,                      │      │                        │
│    sizeof(buf), ...);        │      │                        │
└────────────────┬─────────────┼──────┼────────────────────────┘
                 │             │      │
                 │ p_buffer    │      │ owns lifetime
                 ▼             │      │
        ┌──────────────────────┼──────┼────────────┐
        │ metac_yaml_context_t │      │            │
        │ {                    │      │            │
        │   output: {          │      │            │
        │     p_buffer ────────┘      │            │
        │     buffer_size: 4096       │            │
        │     current_pos: 0          │            │
        │     overflow: 0             │            │
        │   }                         │            │
        │   emitter: {...}            │            │
        │   p_iterator ──────┐        │            │
        │ }                  │        │ on stack   │
        └────────────┬───────┼────────┼────────────┘
                     │       │        │
                     │       ▼        │
            MALLOC'D │   ┌──────────────┐
                     │   │Iterator      │ malloc'ed by
                     │   │{list of      │ metac
                     └──▶│ tasks}       │
                         │(on heap)     │
                         └──────────────┘

KEY INSIGHT:
  - buf[4096]: User owns
  - metac_yaml_context_t: Stack (caller's frame)
  - Iterator: Heap (metac manages, freed at end)
  - No internal output buffering in metac
  - write_handler copies directly: libyaml buffer → user buf
```

## Overflow Handling Flow

```
User wants to serialize large structure but has only 256 bytes:

                ┌─────────────────────────┐
                │ metac_value_to_yaml_    │
                │ buffer(pv, buf, 256...) │
                └────────────┬────────────┘
                             │
                             ▼
            ┌────────────────────────────────┐
            │ Iterator loop starts           │
            │ Emit events, bytes accumulate  │
            │                               │
            │ pos=0 STREAM_START (5 bytes)  │
            │ pos=5 DOC_START (3 bytes)     │
            │ pos=8 MAPPING_START (0 bytes) │
            │ pos=8 SCALAR "x:" (3 bytes)   │
            │ pos=11 SCALAR "10" (2 bytes)  │
            │ ...continues...               │
            │ pos=248 SCALAR "field_x"      │
            │                               │
            │ Next write: +25 bytes needed  │
            │ Check: 248 + 25 > 256?  YES! │
            └────────────┬──────────────────┘
                         │
                         ▼
            ┌────────────────────────────────┐
            │write_handler()                 │
            │  overflow = 1                  │
            │  return 0 (FAIL)               │
            └────────────┬──────────────────┘
                         │
                         ▼
            ┌────────────────────────────────┐
            │libyaml detects failure         │
            │stops emitting                  │
            │                               │
            │returns error                  │
            └────────────┬──────────────────┘
                         │
                         ▼
            ┌────────────────────────────────┐
            │metac_value_to_yaml_impl()      │
            │checks overflow flag            │
            │returns -ENOSPC                 │
            │p_bytes_written = 248           │
            └────────────┬──────────────────┘
                         │
                         ▼
            ┌────────────────────────────────┐
            │User gets back:                 │
            │  err = -ENOSPC                 │
            │  p_bytes_written = 248         │
            │                               │
            │User knows: "need 248 bytes"   │
            │Allocates larger buffer        │
            │Retries                        │
            └────────────────────────────────┘
```

## Zero-Copy: From Value to User Buffer

```
┌─────────────────────────────────────────────┐
│ metac_value_string() produces C string      │
│ e.g., value = "123" (allocated)             │
│                                              │
│ Necessary here because:                    │
│  - metac needs to format numbers/enums     │
│  - YAML events take char* strings          │
│  - No way to avoid this allocation         │
└─────────────────┬──────────────────────────┘
                  │
                  ▼
       "123" (malloc'ed 4 bytes)
       ├─ String on heap
       └─ Must be formatted somewhere
                  │
                  ▼
    ┌─────────────────────────────┐
    │ yaml_scalar_event_init()    │
    │ Takes: char *value          │
    │ libyaml copies it internally│
    │ (we're not responsible)     │
    └─────────────┬───────────────┘
                  │
                  ▼
    ┌─────────────────────────────┐
    │ free(p_string)              │
    │ (ours, not libyaml's)       │
    └─────────────┬───────────────┘
                  │
                  ▼
    ┌─────────────────────────────┐
    │ yaml_emitter_emit()         │
    │ Writes to write_handler()   │
    └─────────────┬───────────────┘
                  │
                  ▼
    ┌─────────────────────────────┐
    │ write_handler():            │
    │  memcpy(user_buf, libyaml   │
    │         data, size)         │
    │                             │
    │ NOW in user buffer:         │
    │ "123" (2nd copy, user owns)│
    └─────────────────────────────┘

Bottom line:
  - Scalars: 1x malloc for formatting
  - Containers: 0 malloc (just events)
  - TOTAL: Minimal vs. naive approach (which would
    buffer entire YAML in metac first)
```
