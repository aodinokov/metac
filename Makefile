# re-definable defaults
METAC=./metac
CFLAGS=-I./include

all:

# build target defined in Makefile by path $(M)
ifneq ($(M),)
include $(M)/Makefile
# include all make-modules. target.mk is entypoint
include mk/*.mk
$(foreach t,$(subst ./,,$(shell cd $(M) && find . -name '*_test.c')),$(call test_rules,$(M),$(t),rules))
$(foreach t,$(rules),$(call meta_rules,$(M),$(t)))
$(foreach t,$(rules),$(call meta_rules_clean,$(M),$(t)))
else
# include all make-modules. target.mk is entypoint
include mk/*.mk
# this test dones't need meta-information - disable it explicitly
REFLECT-src/inherit_test=n
$(foreach t,$(shell find src -name '*_test.c'),$(call test_rules,$(shell pwd),$(t),gl_rules))
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

gl_rules+= \
	src/libmetac.a
TPL-src/libmetac.a+=a_target
IN-src/libmetac.a+= \
	src/entry.o \
	src/entry_cdecl.o \
	src/entry_db.o \
	src/entry_tag.o \
	src/hashmap.o \
	src/iterator.o \
	src/value.o \
	src/value_base_type.o \
	src/value_deep.o \
	src/value_string.o

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
