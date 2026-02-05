# YAML API Usage Examples

## Quick Start

### Convenience API (Simplest)

```c
#include <metac/backend/yaml.h>
#include <stdio.h>

int main() {
    metac_value_t my_value;
    // ... initialize my_value ...
    
    // One-line serialization
    char *yaml_str = metac_value_to_yaml_string(
        &my_value, 
        METAC_VALUE_WALK_MODE_DEEP);
    
    if (yaml_str) {
        printf("Serialized YAML:\n%s\n", yaml_str);
        metac_free_yaml_string(yaml_str);  // Clean up
    }
    
    return 0;
}
```

**Use when**: Simple logging, one-off serialization, you don't care about allocations

---

### Context API (Full Control)

```c
#include <metac/backend/yaml.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    metac_value_t my_value;
    // ... initialize my_value ...
    
    // Step 1: Create context
    metac_yaml_serialization_t *ctx = 
        metac_yaml_serialization_new(&my_value, METAC_VALUE_WALK_MODE_DEEP);
    
    if (!ctx) {
        printf("Failed to create context\n");
        return 1;
    }
    
    // Step 2: Measure required size
    size_t required_size = 0;
    if (metac_yaml_serialization_measure(ctx, &required_size) != 0) {
        printf("Measurement failed\n");
        metac_yaml_serialization_delete(ctx);
        return 1;
    }
    
    printf("Required buffer size: %zu bytes\n", required_size);
    
    // Step 3: Allocate buffer
    char *buffer = (char *)malloc(required_size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        metac_yaml_serialization_delete(ctx);
        return 1;
    }
    
    // Step 4: Serialize to buffer
    if (metac_yaml_serialization_to_buffer(ctx, buffer, required_size) != 0) {
        printf("Serialization failed\n");
        free(buffer);
        metac_yaml_serialization_delete(ctx);
        return 1;
    }
    
    // Step 5: Use the serialized data
    printf("Serialized YAML:\n%s\n", buffer);
    
    // Step 6: Clean up
    free(buffer);
    metac_yaml_serialization_delete(ctx);
    
    return 0;
}
```

**Use when**: Resource-constrained, reusable buffers, multiple outputs, performance critical

---

## Patterns

### Pattern 1: Fire-and-Forget Logging

```c
void log_value(metac_value_t *p_val) {
    char *yaml = metac_value_to_yaml_string(p_val, METAC_VALUE_WALK_MODE_DEEP);
    if (yaml) {
        fprintf(stderr, "DEBUG VALUE:\n%s\n", yaml);
        metac_free_yaml_string(yaml);
    }
}

int main() {
    metac_value_t val;
    log_value(&val);
    return 0;
}
```

---

### Pattern 2: Reusable Context (Multiple Outputs)

```c
int serialize_multiple_values(metac_value_t *p_values, int count) {
    // Allocate once, reuse for all values
    char buffer[4096];
    
    for (int i = 0; i < count; i++) {
        metac_yaml_serialization_t *ctx = 
            metac_yaml_serialization_new(&p_values[i], METAC_VALUE_WALK_MODE_SHALLOW);
        
        if (!ctx) continue;
        
        size_t size = 0;
        if (metac_yaml_serialization_measure(ctx, &size) != 0) {
            metac_yaml_serialization_delete(ctx);
            continue;
        }
        
        if (size > sizeof(buffer)) {
            fprintf(stderr, "Buffer too small\n");
            metac_yaml_serialization_delete(ctx);
            continue;
        }
        
        if (metac_yaml_serialization_to_buffer(ctx, buffer, sizeof(buffer)) == 0) {
            printf("Value %d:\n%s\n", i, buffer);
        }
        
        metac_yaml_serialization_delete(ctx);
    }
    
    return 0;
}
```

---

### Pattern 3: Stack Allocation (Embedded)

```c
int serialize_with_stack_buffer(metac_value_t *p_val, size_t max_size) {
    // On stack - no malloc
    char buffer[1024];
    
    metac_yaml_serialization_t *ctx = 
        metac_yaml_serialization_new(p_val, METAC_VALUE_WALK_MODE_SHALLOW);
    
    if (!ctx) return -1;
    
    size_t required = 0;
    if (metac_yaml_serialization_measure(ctx, &required) != 0) {
        metac_yaml_serialization_delete(ctx);
        return -1;
    }
    
    if (required > sizeof(buffer)) {
        fprintf(stderr, "Value too large for stack buffer\n");
        metac_yaml_serialization_delete(ctx);
        return -1;
    }
    
    if (metac_yaml_serialization_to_buffer(ctx, buffer, sizeof(buffer)) != 0) {
        metac_yaml_serialization_delete(ctx);
        return -1;
    }
    
    printf("%s\n", buffer);
    metac_yaml_serialization_delete(ctx);
    return 0;
}
```

---

### Pattern 4: Error Handling

```c
int safe_yaml_serialize(metac_value_t *p_val, char **p_output) {
    if (!p_val || !p_output) {
        return -1;  // Invalid input
    }
    
    char *yaml = metac_value_to_yaml_string(p_val, METAC_VALUE_WALK_MODE_DEEP);
    
    if (!yaml) {
        *p_output = NULL;
        return -1;  // Serialization failed
    }
    
    *p_output = yaml;
    return 0;  // Success
}

int main() {
    metac_value_t val;
    char *result = NULL;
    
    if (safe_yaml_serialize(&val, &result) == 0) {
        printf("%s\n", result);
        metac_free_yaml_string(result);
    } else {
        printf("Serialization failed\n");
    }
    
    return 0;
}
```

---

### Pattern 5: Shallow vs Deep Traversal

```c
void demo_walk_modes(metac_value_t *p_val) {
    char *shallow = metac_value_to_yaml_string(p_val, METAC_VALUE_WALK_MODE_SHALLOW);
    char *deep = metac_value_to_yaml_string(p_val, METAC_VALUE_WALK_MODE_DEEP);
    
    printf("=== SHALLOW (direct fields only) ===\n");
    if (shallow) printf("%s\n", shallow);
    
    printf("\n=== DEEP (all nested fields) ===\n");
    if (deep) printf("%s\n", deep);
    
    metac_free_yaml_string(shallow);
    metac_free_yaml_string(deep);
}
```

---

## Common Mistakes to Avoid

### ❌ Wrong: Forgetting to free

```c
// MEMORY LEAK
char *yaml = metac_value_to_yaml_string(&val, METAC_VALUE_WALK_MODE_DEEP);
printf("%s\n", yaml);
// yaml not freed! Memory leak!
```

### ✅ Correct: Always free

```c
// CORRECT
char *yaml = metac_value_to_yaml_string(&val, METAC_VALUE_WALK_MODE_DEEP);
if (yaml) {
    printf("%s\n", yaml);
    metac_free_yaml_string(yaml);  // ← Always free!
}
```

---

### ❌ Wrong: Using buffer after delete

```c
// USE-AFTER-FREE
metac_yaml_serialization_t *ctx = 
    metac_yaml_serialization_new(&val, METAC_VALUE_WALK_MODE_DEEP);
metac_yaml_serialization_measure(ctx, &size);
char buf[1024];
metac_yaml_serialization_to_buffer(ctx, buf, sizeof(buf));
metac_yaml_serialization_delete(ctx);

printf("%s\n", buf);  // ← buf still valid (it's on stack)
// But iterator in ctx is freed!
```

### ✅ Correct: Clean up in right order

```c
// CORRECT
metac_yaml_serialization_t *ctx = 
    metac_yaml_serialization_new(&val, METAC_VALUE_WALK_MODE_DEEP);
metac_yaml_serialization_measure(ctx, &size);
char buf[1024];
metac_yaml_serialization_to_buffer(ctx, buf, sizeof(buf));

printf("%s\n", buf);  // ← Use buffer BEFORE delete!

metac_yaml_serialization_delete(ctx);  // ← Delete context last
```

---

### ❌ Wrong: Buffer too small

```c
// OVERFLOW
size_t required_size = 0;
metac_yaml_serialization_measure(ctx, &required_size);

char small_buf[10];  // Too small!
metac_yaml_serialization_to_buffer(ctx, small_buf, sizeof(small_buf));
// Returns -1, partial data in small_buf
```

### ✅ Correct: Check size

```c
// CORRECT
size_t required_size = 0;
metac_yaml_serialization_measure(ctx, &required_size);

if (required_size > 1024) {
    printf("Value too large\n");
    return -1;
}

char buf[1024];
if (metac_yaml_serialization_to_buffer(ctx, buf, sizeof(buf)) != 0) {
    printf("Serialization failed\n");
    return -1;
}
```

---

### ❌ Wrong: Not checking NULL

```c
// NULL DEREFERENCE
char *yaml = metac_value_to_yaml_string(NULL, METAC_VALUE_WALK_MODE_DEEP);
printf("%s\n", yaml);  // ← Crash! yaml is NULL
```

### ✅ Correct: Check return value

```c
// CORRECT
char *yaml = metac_value_to_yaml_string(&val, METAC_VALUE_WALK_MODE_DEEP);
if (yaml) {
    printf("%s\n", yaml);
    metac_free_yaml_string(yaml);
} else {
    printf("Serialization failed\n");
}
```

---

## Performance Tips

### Tip 1: Reuse contexts for similar values

```c
// GOOD: Reuse context structure for multiple values
for (int i = 0; i < 1000; i++) {
    metac_yaml_serialization_t *ctx = 
        metac_yaml_serialization_new(&values[i], METAC_VALUE_WALK_MODE_DEEP);
    // Process...
    metac_yaml_serialization_delete(ctx);
}
```

### Tip 2: Use shallow walk for large nested structures

```c
// FAST: Only serialize top-level fields
char *yaml = metac_value_to_yaml_string(
    &val, 
    METAC_VALUE_WALK_MODE_SHALLOW);  // Much faster for deep structures
```

### Tip 3: Pre-allocate buffers if size is known

```c
// EFFICIENT: Single traversal with cached iterator
size_t size = 0;
metac_yaml_serialization_measure(ctx, &size);
char *buf = malloc(size);  // Exact size, no reallocation
metac_yaml_serialization_to_buffer(ctx, buf, size);
// ... use buf ...
free(buf);
```

### Tip 4: Use stack buffers for small values

```c
// STACK: No malloc overhead for known small values
char buffer[512];
metac_yaml_serialization_to_buffer(ctx, buffer, sizeof(buffer));
```

---

## Testing Your Integration

```c
#include <assert.h>

void test_yaml_api() {
    metac_value_t val;
    // Initialize val...
    
    // Test 1: Convenience API
    char *yaml1 = metac_value_to_yaml_string(&val, METAC_VALUE_WALK_MODE_DEEP);
    assert(yaml1 != NULL);
    assert(strlen(yaml1) > 0);
    
    // Test 2: Context API
    metac_yaml_serialization_t *ctx = 
        metac_yaml_serialization_new(&val, METAC_VALUE_WALK_MODE_DEEP);
    assert(ctx != NULL);
    
    size_t size = 0;
    assert(metac_yaml_serialization_measure(ctx, &size) == 0);
    assert(size > 0);
    
    char *yaml2 = malloc(size);
    assert(metac_yaml_serialization_to_buffer(ctx, yaml2, size) == 0);
    
    // Test 3: Both APIs produce same output
    assert(strcmp(yaml1, yaml2) == 0);
    
    // Cleanup
    free(yaml2);
    metac_yaml_serialization_delete(ctx);
    metac_free_yaml_string(yaml1);
    
    printf("All tests passed!\n");
}
```

---

## See Also

- API Reference: `include/metac/serialization/yaml.h`
- Implementation: `src/serialization/yaml/`
- Tests: `src/serialization/yaml/yaml_test.c`
- Design: `.github/YAML_SERIALIZATION_DESIGN.md`

