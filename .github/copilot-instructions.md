# Metac: AI Coding Agent Instructions

## Project Overview

**Metac** is a C framework that adds **reflection and Golang-like features to ANSI C** using DWARF debugging information. It enables runtime introspection of C types, structures, and functions—similar to Go's `reflect` package.

### Core Architecture

- **C Core** (`src/`): Reflection runtime, value manipulation, iterators, serialization
- **Go Tooling** (`pkg/`): DWARF parsing, code generation, metadatabase construction
- **Build System**: Complex Makefile-based system that extracts DWARF data and generates C reflection code
- **Key Output**: `libmetac.a` - reflection library linked into C applications

## Critical Build & Development Workflows

### Build Reflection Data - The Two-Pass System

Metac's build process is **non-trivial** and essential to understand:

1. **First Pass** (`*.meta.o`): Compile with `-g3 -D_METAC_OFF_` to capture full DWARF debug info (reflection disabled to avoid circular deps)
2. **DWARF Extraction**: Go tool (`metac-reflect-gen`) parses DWARF from compiled objects, generates `*.reflect.c`
3. **Second Pass**: Recompile with generated reflection code, link `libmetac.a`
4. **macOS Special**: Requires `dsymutil` after linking to extract DWARF from final binary

**Key Files:**
- [mk/metac.mk](mk/metac.mk) - Core build rules (%.meta.o, %.reflect.c, METAC_DWARFY_SRC platform detection)
- [Makefile](Makefile) - libmetac.a build definition, example rules
- [doc/demo/README.md](doc/demo/README.md) - Explains two-pass system in detail

### Testing

- **C Tests**: `*_test.c` files auto-discovered; compiled with `metac-test-gen` module; use `check` framework
- **Go Tests**: `go test ./...` in `pkg/` directory
- **Build Tests**: `make test` runs all tests
- **Coverage**: `RUNMODE=coverage make test`

### Project Build Commands

```bash
make              # Build metac binary + libmetac.a + examples
make test         # Run all tests (C + Go)
make examples     # Build all examples in examples/ and doc/demo/
make doc          # Generate Doxygen documentation
```

## Project-Specific Conventions

### Metac Macros & Patterns

Use these to create reflectable variables:

```c
// Wrap variable declarations to capture type info at compile-time
WITH_METAC_DECLLOC(decl_location,
    struct test t = { .y = -10, .c = 'a' };
)
metac_value_t *p_val = METAC_VALUE_FROM_DECLLOC(decl_location, t);
```

Key patterns:
- `WITH_METAC_DECLLOC` / `METAC_VALUE_FROM_DECLLOC` - Capture variable metadata
- `METAC_GSYM_LINK_ENTRY(fn)` - Reference functions for reflection
- `metac_value_walk_mode_t` - Controls deep vs shallow value traversal
- `_METAC_OFF_` flag - Disables reflection macros during DWARF-only compilation

### Makefile Rules (Variable Template System)

Makefiles use a **variable-based templating system** (not typical Make syntax):

```makefile
# Define rule components via variables
TPL-target:=bin_target          # Template type
IN-target= main.o other.o       # Input files
LDFLAGS-target=-Lsrc -lmetac    # Link flags
DEPS-target=src/libmetac.a      # Build dependencies
METACFLAGS-target+=run metac-reflect-gen    # Code generation

# For reflection generation
TPL-main.reflect.c:=metac_target    # Special code-gen template
IN-main.reflect.c=_meta_program      # Input: object with DWARF
METACFLAGS-main.reflect.c+=run metac-reflect-gen
```

**Key template types:** `bin_target`, `a_target`, `phony_target`, `metac_target`, `metac_test_target`

See [mk/rules.mk](mk/rules.mk) for rule definitions.

### Module System (pkg/)

Go modules are loaded by `metac` binary and executed via templates:

- **metac-reflect-gen**: Generates `*.reflect.c` from DWARF data (templates in `modules/metac-reflect-gen/templates/`)
- **metac-test-gen**: Generates test runners
- **Module config**: YAML specification defines inputs, templates, outputs

Entry point: [cmd/run/run.go](cmd/run/run.go) - Cobra CLI runner

### Code Generation (Go Templates)

- Generator templates use **Go text/template** syntax in `.tpl` files
- Custom functions: `dwarfToMetaDb`, `snakecase`, `toJson`, etc. in `pkg/module/gotemplate/funcs/`
- Example: [modules/metac-reflect-gen/templates/_.tpl](modules/metac-reflect-gen/templates/_.tpl) - Main entry

## Essential Reference Architecture

### Reflection Data Structures (`include/metac/reflect/`)

- **Entry** - Type descriptor (struct, function, variable, etc.)
- **Value** - Runtime instance with entry metadata
- **Entry DB** - Lookup table of entries by name/location
- **Link** - Reference to entry with external linkage

### Serialization Backends

Located in `include/metac/serialization/` and [src/serialization/](src/serialization/):
- **YAML** - Event-driven API with user-controlled memory:
  - `metac_value_to_yaml_buffer()` - Zero-malloc serialization to user buffer
  - Uses `yaml_emitter_set_output()` with custom write handler
  - Iterator manages YAML event generation (no internal buffering)
  - See [.github/YAML_SERIALIZATION_DESIGN.md](.github/YAML_SERIALIZATION_DESIGN.md) for architecture
- **JSON** - Similar pair  
- Backends access values via `metac_value_walk_mode_t` (shallow/deep)

### Deep Operations

`metac_value_deep_*` functions operate on complex nested structures:
- `metac_value_deep_copy()` - Clone with full hierarchy
- `metac_value_deep_cmp()` - Compare recursively
- `metac_value_deep_free()` - Release all allocations
- `metac_value_print_string()` - Format like `{.field = value}`

### Recursive Iterator (Stack-Free Traversal)

Located in `include/metac/backend/iterator.h` and [src/iterator.c](src/iterator.c):

The iterator provides **heap-based traversal** of complex structures without stack limitation:
- **State machine**: Each task has states (start → user code → dependency resolution → done/failed)
- **Dependencies**: Create sub-tasks for array elements, struct fields, nested types
- **Workflow**: Main loop calls `metac_recursive_iterator_next()`, examines task, optionally creates deps, updates state
- **Memory**: Iterator maintains task queue; caller responsible for processing results
- **Used in**: YAML/JSON serialization (event generation per field), deep operations

Key functions:
- `metac_new_recursive_iterator(p_in)` - Start traversal
- `metac_recursive_iterator_next()` - Get next task
- `metac_recursive_iterator_create_and_append_dep()` - Add sub-task
- `metac_entry_iterator_set_state()` / `metac_entry_iterator_get_state()` - Manage state machine
- `metac_recursive_iterator_done()` / `metac_recursive_iterator_fail()` - Complete task

## Integration Points & External Dependencies

### DWARF Processing Pipeline

```
Binary/Object → pkg/dwarfy/ → DWARF structs → pkg/metadb/ → MetaDb → Templates → C Code
```

- [pkg/dwarfy/dwarf.go](pkg/dwarfy/dwarf.go) - DWARF parsing from ELF/PE/Mach-O
- [pkg/metadb/metadb_builder.go](pkg/metadb/metadb_builder.go) - Normalizes DWARF → structured MetaDb

### YAML Serialization (Zero-Malloc)

Metac provides event-driven YAML serialization controlled by user memory:
- **Quick Start**: [.github/YAML_QUICK_START.md](.github/YAML_QUICK_START.md) - API overview and examples
- **Design**: [.github/YAML_SERIALIZATION_DESIGN.md](.github/YAML_SERIALIZATION_DESIGN.md) - Architecture and API patterns
- **Implementation**: [.github/YAML_IMPLEMENTATION_GUIDE.md](.github/YAML_IMPLEMENTATION_GUIDE.md) - Code structure and checklist
- **Data Flows**: [.github/YAML_DATAFLOW_DIAGRAMS.md](.github/YAML_DATAFLOW_DIAGRAMS.md) - Visual flow and memory layout

**Key concept**: Iterator yields value tasks → custom write handler copies events directly to user buffer → zero internal malloc

### Platform Detection

[mk/metac.mk](mk/metac.mk#L18) sets:
- Linux: `METAC_DWARFY_SRC=elf` (default)
- Windows: `METAC_DWARFY_SRC=pe` (PE format for object files)
- macOS: `METAC_DWARFY_SRC=macho`, requires `dsymutil` post-processing

### External Libraries

- `check` framework - C unit testing
- `pkg-config` - Detect check library
- Go dependencies: sprig (templating), cobra (CLI), yaml.v3 (parsing)

## Common Tasks & Their Locations

| Task | Location | Command |
|------|----------|---------|
| Add C runtime function | [src/](src/) | Edit `.c` file, update include guards |
| Add reflection to struct | Your app Makefile | Add `TPL-app.reflect.c:=metac_target` rule |
| Generate test scaffolding | Code generation | `make test` discovers `*_test.c` automatically |
| Add Go module | [modules/](modules/) | Create `module.yaml` + templates/ |
| Fix platform DWARF issues | [mk/metac.mk](mk/metac.mk#L18-L30) | Update `METAC_DWARFY_SRC` or `POST-` steps |
| Add example | [examples/](examples/) | Create folder with Makefile following template pattern |
| Debug DWARF parsing | [pkg/dwarfy/testdata/](pkg/dwarfy/testdata/) | Add DWARF test case |

## Quick Start for New Features

1. **New C library function**: Add to [src/](src/), declare in `include/metac/`, update tests
2. **Extend reflection**: Modify [pkg/metadb/](pkg/metadb/) → [modules/metac-reflect-gen/templates/](modules/metac-reflect-gen/templates/)
3. **New serialization format**: Add backend to [src/serialization/](src/serialization/), declare in header, implement `to_format`/`from_format`
4. **Build system fix**: Edit [mk/](mk/) Make rules, test with `M=examples/c_app make`

## Documentation Hub

All architectural guidance and design specs in `.github/`:
- **[.github/README.md](.github/README.md)** - Navigation guide for this directory
- **[.github/YAML_QUICK_START.md](.github/YAML_QUICK_START.md)** - API reference and examples
- **[.github/YAML_SERIALIZATION_DESIGN.md](.github/YAML_SERIALIZATION_DESIGN.md)** - Architecture and design
- **[.github/YAML_TECHNICAL_SPECIFICATION.md](.github/YAML_TECHNICAL_SPECIFICATION.md)** - Formal spec and acceptance criteria
