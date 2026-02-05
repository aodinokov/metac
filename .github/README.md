# Metac Documentation - `.github` Directory

This directory contains comprehensive architecture, design, and implementation guidance for Metac AI agents and developers.

## Quick Navigation

### 🚀 Getting Started
- **[copilot-instructions.md](copilot-instructions.md)** - Start here! Overview of project structure, build system, conventions, and key patterns. Updated to reflect latest YAML serialization work.

### 📋 Latest: YAML API with Shared Code Architecture

**Answer to "Why is code duplicated?"**: It's not! Two APIs share a single core with **90% code reuse**.

**Quick entry**:
- **💡 Understand the architecture**: [SHARED_CODE_COMPLETION.md](SHARED_CODE_COMPLETION.md) - Summarizes how two APIs share one implementation
- **📊 See code organization**: [SHARED_CODE_ARCHITECTURE.md](SHARED_CODE_ARCHITECTURE.md) - Diagrams and statistics

### 📋 YAML Serialization Design

The YAML serialization subsystem is architected for **zero-malloc, user-controlled memory** with two complementary APIs sharing a single core implementation.

**Quick orientation:**
- **Want quick reference?** → [YAML_API_REFERENCE_CARD.md](YAML_API_REFERENCE_CARD.md) (2 min cheat sheet)
- **Want architecture overview?** → [SHARED_CODE_ARCHITECTURE.md](SHARED_CODE_ARCHITECTURE.md) ⭐ (5 min)
- **Want to use the API?** → [YAML_QUICK_START.md](YAML_QUICK_START.md) ⭐ (5 min)
- **Want to implement it?** → [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) (10 min)

**Full reading order:**

1. **[SHARED_CODE_COMPLETION.md](SHARED_CODE_COMPLETION.md)** ⭐ (5 min - completion summary)
   - Question answered: "Will convenience have separate code?"
   - Answer: No! 90% shared core
   - Code statistics and organization
   - Maintenance benefits
   - Implementation examples

2. **[YAML_API_REFERENCE_CARD.md](YAML_API_REFERENCE_CARD.md)** (2 min - quick reference)
   - Two APIs side-by-side
   - When to use each
   - Common patterns
   - Implementation details

3. **[SHARED_CODE_ARCHITECTURE.md](SHARED_CODE_ARCHITECTURE.md)** (5 min - architecture)
   - Two APIs, one core principle
   - Visual architecture diagrams
   - Code statistics and comparison
   - Maintenance benefits
   - **Best for understanding the design philosophy**

4. **[YAML_QUICK_START.md](YAML_QUICK_START.md)** (5 min read)
   - Context API (measure + buffer)
   - Convenience API (fire-and-forget)
   - 4 usage patterns
   - Error handling

3. **[YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md)** (10 min read - implementer focused)
   - File structure and organization
   - Shared vs wrapper code breakdown
   - Implementation flow for both APIs
   - Maintenance scenarios
   - Testing strategy
   - Build configuration

4. **[YAML_API_DESIGN_RATIONALE.md](YAML_API_DESIGN_RATIONALE.md)** (10 min read)
   - Why two APIs?
   - Single vs multiple traversals problem
   - Shared code architecture principle
   - Design hierarchy and API selection

5. **[YAML_SERIALIZATION_DESIGN.md](YAML_SERIALIZATION_DESIGN.md)** (15 min read)
   - Complete API with prototypes
   - Context structure design
   - Write handler mechanism
   - Main event loop with code
   - Usage example

6. **[YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md)** (20 min read)
   - Header file structure
   - Context initialization code
   - Event generation loop (full code)
   - Convenience function (wrapper code)
   - Testing patterns

7. **[YAML_DATAFLOW_DIAGRAMS.md](YAML_DATAFLOW_DIAGRAMS.md)** (10 min read)
   - High-level architecture diagram
   - Iterator state transitions
   - Memory flow visualization
   - Overflow handling sequence
   - Zero-copy explanation

8. **[YAML_TECHNICAL_SPECIFICATION.md](YAML_TECHNICAL_SPECIFICATION.md)** (30 min read)
   - Complete formal specification
   - Problem analysis and requirements
   - 3-layer architecture details
   - Full API specification with return values
   - Error handling strategy
   - Performance characteristics
   - Acceptance criteria checklist

---

## Key Architectural Concepts

### Two-Pass Build System
Metac uses DWARF debugging information for reflection. The build requires two compilation passes:
1. **First Pass**: Compile with `-g3 -D_METAC_OFF_` to extract DWARF (without reflection macros)
2. **DWARF Extraction**: Go tool parses DWARF, generates reflection C code
3. **Second Pass**: Recompile with generated code and link `libmetac.a`

**Learn more**: [copilot-instructions.md](copilot-instructions.md#build-reflection-data---the-two-pass-system)

### Iterator State Machine (Heap-Based Recursion)
Avoids stack overflow on deeply nested structures by managing traversal as heap task queue:
- Each value/field is a task with states: start → work → done/failed
- Tasks can create sub-tasks (dependencies)
- Iterator maintains queue; caller processes one task per iteration

**Used by**:
- YAML/JSON serialization (generate one event per task)
- Deep operations (copy, compare, free)

**Learn more**: [copilot-instructions.md](copilot-instructions.md#recursive-iterator-stack-free-traversal)

### Zero-Malloc Serialization (Event-Driven)
YAML serialization designed for user memory control:
- User provides output buffer
- metac acts as event generator (via iterator)
- libyaml emitter handles YAML syntax
- Custom write handler copies directly to user buffer
- No internal buffering in metac

**Two complementary APIs, single shared core**:
- **Context API**: Explicit measure + serialize, reusable for multiple outputs
- **Convenience API**: Single call, internal malloc, simpler code
- Both use identical implementation (90% code reuse)

**Learn more**: 
- [YAML_QUICK_START.md](YAML_QUICK_START.md) - Quick overview
- [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) - Implementation architecture
- [YAML_SERIALIZATION_DESIGN.md](YAML_SERIALIZATION_DESIGN.md) - Full design

---

## Architecture Highlights: Shared Code Principle

### YAML Serialization: Two APIs, One Core

```
┌─────────────────────────────────────┐
│  Public User APIs                   │
├─────────────────────────────────────┤
│                                     │
│  Context API:                       │  Convenience API:
│  - new()                            │  - to_yaml_string()
│  - measure()                        │    (wrapper ~28 lines)
│  - to_buffer()                      │
│  - delete()                         │
│                                     │
│  ↓ (explicit calls)        ↓ (calls internally)
│
└──────────┬──────────────────────────┘
           │
           ↓
    ┌──────────────────────┐
    │  Shared Core Code    │
    │  (250 lines)         │
    │                      │
    │  - Context lifecycle │
    │  - Iterator caching  │
    │  - Event generation  │
    │  - libyaml handler   │
    └──────────────────────┘

Maintenance: Bug fix in core → both APIs fixed automatically!
```

**Code reuse**: 90% shared, 10% wrapper glue

**See also**: [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md) for implementation details

---

## File Structure Reference

```
.github/
├── README.md (this file) - Navigation hub
├── copilot-instructions.md - Project overview for AI agents
│
├── YAML Serialization (10 documents)
│   ├── YAML_API_REFERENCE_CARD.md ⭐ Quick reference
│   ├── SHARED_CODE_ARCHITECTURE.md ⭐ Architecture overview
│   ├── YAML_API_DESIGN_RATIONALE.md - Why two APIs?
│   ├── YAML_CODE_ORGANIZATION.md - File structure & implementation
│   ├── YAML_QUICK_START.md - API usage guide
│   ├── YAML_SERIALIZATION_DESIGN.md - Complete design
│   ├── YAML_IMPLEMENTATION_GUIDE.md - Code templates
│   ├── YAML_DATAFLOW_DIAGRAMS.md - Memory flow diagrams
│   ├── YAML_TECHNICAL_SPECIFICATION.md - Formal spec
│   └── YAML_QUICK_START.md - Examples & patterns
│
├── ROADMAP.md - Future work and priorities
├── DELIVERY_SUMMARY.md - What was delivered
├── COMPLETION.txt - Checklist of completed items
│
└── (Planned: JSON, DWARF, Build System docs)
```

---

## Documentation Relationships

```
Quick Start Path (New User):
  README.md ↓
  YAML_API_REFERENCE_CARD.md ↓
  YAML_QUICK_START.md ↓
  Examples ✓

Architecture Path (Designer/Reviewer):
  README.md ↓
  SHARED_CODE_ARCHITECTURE.md ↓
  YAML_API_DESIGN_RATIONALE.md ↓
  YAML_SERIALIZATION_DESIGN.md ✓

Implementation Path (Developer):
  README.md ↓
  YAML_CODE_ORGANIZATION.md ↓
  YAML_IMPLEMENTATION_GUIDE.md ↓
  Write code ✓

Deep Dive Path (Maintainer):
  All of above +
  YAML_TECHNICAL_SPECIFICATION.md +
  YAML_DATAFLOW_DIAGRAMS.md ✓
```

---

## For Different Audiences

### 👨‍💻 **Developers Adding Features**
1. Read: [copilot-instructions.md](copilot-instructions.md)
2. Quick ref: [YAML_API_REFERENCE_CARD.md](YAML_API_REFERENCE_CARD.md)
3. For YAML work: [YAML_QUICK_START.md](YAML_QUICK_START.md) → [YAML_CODE_ORGANIZATION.md](YAML_CODE_ORGANIZATION.md)
4. Implement: Use [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md) templates

### 🤖 **AI Coding Agents (Copilot, Claude, etc.)**
1. Read: [copilot-instructions.md](copilot-instructions.md) (complete overview)
2. For deep work: Reference specific `.md` docs as needed
3. When implementing YAML features: Use [YAML_TECHNICAL_SPECIFICATION.md](YAML_TECHNICAL_SPECIFICATION.md) as acceptance criteria

### 📚 **New Project Members**
1. Start: [copilot-instructions.md](copilot-instructions.md)
2. Build a hello-world app: See examples in `doc/demo/step_00` - `step_07`
3. For subsystems: Dive into corresponding design doc

### 🔍 **Code Reviewers**
1. Check against: [copilot-instructions.md](copilot-instructions.md) conventions
2. For YAML work: Verify against [YAML_TECHNICAL_SPECIFICATION.md](YAML_TECHNICAL_SPECIFICATION.md) acceptance criteria
3. Memory safety: Review [YAML_DATAFLOW_DIAGRAMS.md](YAML_DATAFLOW_DIAGRAMS.md) memory flow section

---

## Design Philosophy

### Principles
1. **User Control**: Memory allocation is user's responsibility → predictable behavior
2. **Stack Safety**: Heap-based iteration → handle arbitrary nesting depth
3. **Event-Driven**: Iterator + emitter model → composable backends
4. **Minimal Malloc**: Only allocate what can't be avoided (parsing, formatting)
5. **DWARF as Source of Truth**: Reflection info generated from debug symbols

### Trade-offs Made
- **Complexity vs Safety**: Added iterator state machine to avoid stack limits
- **Flexibility vs Simplicity**: User must manage buffer lifetime (clearer ownership)
- **Allocation Timing**: Allocate buffer upfront (easier than streaming)

---

## Common Tasks

| Task | Document | Time |
|------|----------|------|
| Understand project structure | [copilot-instructions.md](copilot-instructions.md) | 5 min |
| Add YAML serialization feature | [YAML_IMPLEMENTATION_GUIDE.md](YAML_IMPLEMENTATION_GUIDE.md) | 30 min |
| Debug memory leak in serialization | [YAML_DATAFLOW_DIAGRAMS.md](YAML_DATAFLOW_DIAGRAMS.md) | 15 min |
| Understand iterator behavior | [copilot-instructions.md](copilot-instructions.md#recursive-iterator-stack-free-traversal) | 10 min |
| Extend to JSON backend | Start with YAML docs, apply pattern | 1+ hour |
| Implement YAML deserialization | [YAML_TECHNICAL_SPECIFICATION.md](YAML_TECHNICAL_SPECIFICATION.md#future-enhancements) | Planning |

---

## Maintenance & Updates

### When to Update Docs

- **New major feature**: Create `FEATURE_DESIGN.md` documenting architecture
- **Build system change**: Update [copilot-instructions.md](copilot-instructions.md#critical-build--development-workflows)
- **API change**: Update relevant `.md` + acceptance criteria
- **New convention discovered**: Add to [copilot-instructions.md](copilot-instructions.md#project-specific-conventions)

### Review Checklist

When updating docs:
- [ ] Links all work (relative paths only)
- [ ] Code examples compile and run
- [ ] Diagrams are clear and up-to-date
- [ ] Cross-references consistent
- [ ] Matches actual codebase structure
- [ ] Tested against current `main` branch

---

## Document Formats & Conventions

### Code Examples
- C code uses full context (includes, function sig)
- Go code shows package context
- Pseudo-code uses clear English
- All examples must be buildable/runnable

### Diagrams
- ASCII diagrams for flow and architecture
- Box-based for component relationships
- Arrow-based for data/control flow
- Keep simple and scannable

### Links
- **Internal**: `[text](FILENAME.md)` or `[text](FILENAME.md#section)`
- **Code**: `[text](../../path/to/file.c#L123)`
- **External**: Full URL, describe resource

### Markdown Style
- H1 (`#`) = Document title
- H2 (`##`) = Major section
- H3 (`###`) = Subsection
- Tables for comparisons
- Lists for unordered items
- Code fences with language tag

---

## Related Resources

### In Repository
- **Examples**: `examples/` - Working sample applications
- **Demos**: `doc/demo/step_00` through `step_07` - Tutorial walkthrough
- **Tests**: `src/*_test.c` - Unit tests as usage examples
- **Makefile**: `mk/` - Build system rules and patterns

### External
- **libyaml**: http://pyyaml.org/wiki/LibYAML - YAML C library
- **DWARF**: http://dwarfstd.org/ - Debugging information format
- **Go Reflect**: https://pkg.go.dev/reflect - Inspiration for metac API

---

## Questions? Feedback?

If documentation is unclear:
1. Check cross-references (might be in another doc)
2. Review actual code in `src/` or `include/`
3. Look at working examples in `examples/` or `doc/demo/`
4. Ask in PR/issue with specific question

If updating docs, ensure they match code reality!

---

**Last Updated**: February 5, 2026  
**Status**: Active (YAML subsystem docs complete)
