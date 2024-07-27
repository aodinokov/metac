# re-definable defaults
METAC=./metac
CFLAGS=-I./include

all:

# build target defined in Makefile by path $(M)
ifneq ($(M),)
include $(M)/Makefile
# include all make-modules. target.mk is entypoint
include mk/*.mk
$(foreach t,$(subst ./,,$(shell cd $(M) && find . -name '*_test.c')),$(call test_c_rules,$(M),$(t),rules))
$(foreach t,$(subst ./,,$(shell cd $(M) && find . -name '*.checkmk')),$(call test_checkmk_rules,$(M),$(t),rules))
$(foreach t,$(rules),$(call meta_rules,$(M),$(t)))
$(foreach t,$(rules),$(call meta_rules_clean,$(M),$(t)))
else
# include all make-modules. target.mk is entypoint
include mk/*.mk
# this test dones't need meta-information - disable it explicitly
REFLECT-src/inherit_test=n
$(foreach t,$(shell find src -name '*_test.c'),$(call test_c_rules,$(shell pwd),$(t),gl_rules))
$(foreach t,$(shell find src -name '*.checkmk'),$(call test_checkmk_rules,$(shell pwd),$(t),gl_rules))
$(foreach m,$(wildcard modules/*),$(foreach t,$(subst $(m)/,,$(shell find $(m) -name '*_test.c.yaml')),$(call modules_test,$(m),$(t))))
$(foreach m,$(wildcard modules/*),$(foreach t,$(subst $(m)/,,$(shell find $(m) -name '*_test.c.yaml')),$(call modules_test_clean,$(m),$(t))))
test: go_test

#add examples/demos as example rule to all
$(foreach e,$(wildcard examples/*),$(call examples_rules,$(e),$(subst examples/,,$(e))))
$(foreach e,$(filter-out %.md %step_00,$(wildcard doc/demo/*)),$(call examples_rules,$(e),$(subst doc/demo/,,$(e))))
all: examples
clean: examples_clean
test: examples_test
#add metac to all
all: metac
clean: RMFLAGS+=metac
#add src/libmetac.a
all: src/libmetac.a test

# doc: TODO: probably should locate Doxygen as Makefile if $(M) is set
doc: Doxyfile
	doxygen $<
.PHONY: doc
endif

# some global rules that we may add for modules so they could depend on them
metac: $(filter-out _test.go, $(shell find ./ -name '*.go')) go.mod go.sum
	go build
ifeq ($(RUNMODE), coverage)
go_test:
	go test --cover ./...
else
go_test:
	go test ./...
endif
.PHONY: go_test

libmetac+= \
	src/entry.c \
	src/entry_cdecl.c \
	src/entry_db.c \
	src/entry_tag.c \
	src/hashmap.c \
	src/iterator.c \
	src/printf_format.c \
	src/value.c \
	src/value_base_type.c \
	src/value_deep.c \
	src/value_string.c \
	src/value_with_args.c

gl_rules+= \
	src/libmetac.a \
	src/_meta_libmetac \
	src/libmetac.reflect.c
TPL-src/libmetac.a+=a_target
IN-src/libmetac.a+= \
	$(libmetac:%.c=%.o) \
	src/libmetac.reflect.o
INCFLAGS-src/libmetac.a+=-DMETAC_MODULE_NAME=libmetac
# this is to genearte
TPL-src/_meta_libmetac+=bin_target
IN-src/_meta_libmetac= \
	$(libmetac:%.c=%.meta.o) \
	src/_module.o
INCFLAGS-src/_meta_libmetac+=-DMETAC_MODULE_NAME=libmetac
POST-src/_meta_libmetac=$(METAC_POST_META)

TPL-src/libmetac.reflect.c+=metac_target
METACFLAGS-src/libmetac.reflect.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-src/libmetac.reflect.c=src/_meta_libmetac

$(foreach t,$(gl_rules),$(call meta_rules,$(shell pwd),$(t)))
ifeq ($(M),)
#clean major componants only if Make is ran for major module
$(foreach t,$(gl_rules),$(call meta_rules_clean,$(shell pwd),$(t)))
endif

src/libmetac.a:$(shell pwd)/src/libmetac.a

# rule to clean files set in variable $(RMFLAGS)
# we could use rm $(RMFLAGS), but there could be dups
clean:
	-echo $(RMFLAGS) | xargs -n 1| sort -u| xargs rm

.PHONY: all clean
