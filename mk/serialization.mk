# Default all WITH_ flags to 0 if not set
WITH_CJSON ?= 0
WITH_YAML ?= 0 # For future use
WITH_JSON_C ?= 0

# --- cJSON Backend ---
ifeq ($(WITH_CJSON),1)
  # for ubuntu needs apt-get install libcjson-dev
  # Add sources to the main library build
  libmetac+=src/serialization/cjson/value_to_cjson.c src/serialization/cjson/value_from_cjson.c

  # Add pkg-config flags
  CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null) -DWITH_CJSON
  CJSON_LIBS := $(shell pkg-config --libs libcjson 2>/dev/null)

  # Append to global flags.
  ifneq ($(CJSON_CFLAGS),)
    CFLAGS += $(CJSON_CFLAGS)
    LDFLAGS += $(CJSON_LIBS)
  else
    $(warning libcjson not found by pkg-config, cJSON backend may fail to link)
  endif
endif

# --- YAML Backend ---
ifeq ($(WITH_YAML),1)
  # Add sources to the main library build
  libmetac+= \
	src/serialization/yaml/yaml_ser_context.c \
	src/serialization/yaml/yaml_ser_handler.c \
	src/serialization/yaml/yaml_ser_events.c \
	src/serialization/yaml/yaml_deser_context.c \
	src/serialization/yaml/yaml_deser_events.c \
	src/serialization/yaml/yaml_deser_api.c \
	src/serialization/yaml/api.c
  # src/serialization/yaml/value_to_yaml.c src/serialization/yaml/value_from_yaml.c

  # Add pkg-config flags
  YAML_CFLAGS := $(shell pkg-config --cflags yaml-0.1 2>/dev/null) -DWITH_YAML
  YAML_LIBS := $(shell pkg-config --libs yaml-0.1 2>/dev/null)

  # Append to global flags.
  ifneq ($(YAML_CFLAGS),)
    CFLAGS += $(YAML_CFLAGS)
    LDFLAGS += $(YAML_LIBS)
  else
    $(warning libyaml not found by pkg-config, YAML backend may fail to link)
  endif
endif

# --- JSON-C Backend ---
ifeq ($(WITH_JSON_C),1)
  # Add sources to the main library build
  libmetac+= \
	src/serialization/json_c/json_c_ser_context.c \
	src/serialization/json_c/json_c_ser_builder.c \
	src/serialization/json_c/json_c_ser_api.c \
	src/serialization/json_c/json_c_deser_context.c \
	src/serialization/json_c/json_c_deser_parser.c \
	src/serialization/json_c/json_c_deser_api.c

  # Add pkg-config flags
  JSON_C_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null) -DWITH_JSON_C
  JSON_C_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)

  # Append to global flags.
  ifneq ($(JSON_C_CFLAGS),)
    CFLAGS += $(JSON_C_CFLAGS)
    LDFLAGS += $(JSON_C_LIBS)
  else
    $(warning json-c not found by pkg-config, JSON-C backend may fail to link)
  endif
endif
