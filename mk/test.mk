# extra template to dummy entrypoint and remove coverage files on cleanup
define test_c_dummy_main_tpl
$$(addprefix $1/,$2.dummy.c):
	@echo "int main(int argc, char **argv ){return 0;}" > $$@
endef
test_c_dummy_main = $(eval $(call test_c_dummy_main_tpl,$1,$2))

define test_c_dummy_main_clean_tpl
clean: RMFLAGS+=$$(addprefix $1/,$2.gcda) $$(addprefix $1/,$2.gcno) $$(addprefix $1/,$2.dummy.c)
endef
test_c_dummy_main_clean = $(eval $(call test_c_dummy_main_clean_tpl,$1,$2))

define test_c_meta_rules_tpl
$(3)+= \
	$(2:.c=) \
	$(2:.c=.reflect.c) \
	$(2:.c=.test.c) \
	$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))

TPL-$(2:.c=):=bin_target test_c_dummy_main
IN-$(2:.c=)=$(2:.c=.o) $(2:.c=.test.o)
LDFLAGS-$(2:.c=)+=--coverage $$(METAC_CHECK_LDFLAGS)
$$(addprefix $1/,$(2:.c=.test.o)):CFLAGS+=-g3 $$(METAC_CHECK_CFLAGS)
$$(addprefix $1/,$(2:.c=.o)):CFLAGS+=-g3 -Wno-format-extra-args --coverage $$(METAC_CHECK_CFLAGS)
$$(addprefix $1/,$(2:.c=.meta.o)):CFLAGS+=-Wno-format-extra-args $$(METAC_CHECK_CFLAGS)
ifneq ($$(REFLECT-$(2:.c=)),n)
IN-$(2:.c=)+=$(2:.c=.reflect.o)

TPL-$(2:.c=.reflect.c):=metac_target
METACFLAGS-$(2:.c=.reflect.c)+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-$(2:.c=.reflect.c)=$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))
endif

TPL-$(2:.c=.test.c):=metac_target
METACFLAGS-$(2:.c=.test.c)+=run metac-test-gen $(METAC_OVERRIDE_IN_TYPE)
IN-$(2:.c=.test.c)=$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))

TPL-$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=)):=bin_target
IN-$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))=$(2:.c=.meta.o) $(2:.c=.dummy.o)
LDFLAGS-$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))=$$(LDFLAGS-$(2:.c=))
DEPS-$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))=$$(DEPS-$(2:.c=))
POST-$$(dir $(2:.c=))_meta_$$(notdir $(2:.c=))=$$(METAC_POST_META)

bin_test: $$(addprefix $1/,$(2:.c=))

endef # test_c_meta_rules_tpl
test_c_rules = $(eval $(call test_c_meta_rules_tpl,$1,$2,$3))

# checkmk support
# extra template to convert checkmk to c using checkmk
define test_checkmk_c_tpl
$$(addprefix $1/,$2.c): $$(addprefix $1/,$(subst _checkmk,.checkmk,$2))
	checkmk $$^ > $$@
endef
test_checkmk_c = $(eval $(call test_checkmk_c_tpl,$1,$2))

define test_checkmk_c_clean_tpl
clean: RMFLAGS+=$$(addprefix $1/,$2.gcda) $$(addprefix $1/,$2.gcno) $$(addprefix $1/,$2.c)
endef
test_checkmk_c_clean = $(eval $(call test_checkmk_c_clean_tpl,$1,$2))

define test_checkmk_meta_rules_tpl
$(3)+= \
	$(2:.checkmk=_checkmk)

TPL-$(2:.checkmk=_checkmk):=bin_target test_checkmk_c
IN-$(2:.checkmk=_checkmk)=$(2:.checkmk=_checkmk.o)
LDFLAGS-$(2:.checkmk=_checkmk)+=--coverage $$(METAC_CHECK_LDFLAGS)
$$(addprefix $1/,$(2:.checkmk=_checkmk.o)):CFLAGS+=-g3 -Wno-format-extra-args --coverage $$(METAC_CHECK_CFLAGS)
$$(addprefix $1/,$(2:.checkmk=_checkmk.meta.o)):CFLAGS+=-Wno-format-extra-args $$(METAC_CHECK_CFLAGS)

ifneq ($$(REFLECT-$(2:.checkmk=_checkmk)),n)
IN-$(2:.checkmk=_checkmk)+=$(2:.checkmk=_checkmk.reflect.o)

TPL-$(2:.checkmk=_checkmk.reflect.c):=metac_target
METACFLAGS-$(2:.checkmk=_checkmk.reflect.c)+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-$(2:.checkmk=_checkmk.reflect.c)=$$(dir $(2:.c=))_meta_$$(notdir $(2:.checkmk=_checkmk))

TPL-$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk)):=bin_target
IN-$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk))=$(2:.checkmk=_checkmk.meta.o)
LDFLAGS-$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk))=$$(LDFLAGS-$(2:.checkmk=))
DEPS-$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk))=$$(DEPS-$(2:.checkmk=))
POST-$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk))=$$(METAC_POST_META)

$(3)+= \
	$(2:.checkmk=_checkmk.reflect.c) \
	$$(dir $(2:.checkmk=_checkmk))_meta_$$(notdir $(2:.checkmk=_checkmk))
endif

bin_test: $$(addprefix $1/,$(2:.checkmk=_checkmk))

endef # test_checkmk_meta_rules_tpl
test_checkmk_rules = $(eval $(call test_checkmk_meta_rules_tpl,$1,$2,$3))

ifneq ($(INCLUDE),)
RUNFILTERFN:=filter
RUNFILTER:=$(INCLUDE)
else
RUNFILTERFN:=filter-out
RUNFILTER:=$(EXCLUDE)
endif


ifeq ($(RUNMODE), valgrind)
# valgrind test target
VALGRIND=$(shell which valgrind)
ifeq ($(VALGRIND), )
$(error Couldn't find valgrind in PATH. Please specify the path manually using VALGRIND param)
endif

bin_test:
	@exitCode=0; \
	for j in $(call $(RUNFILTERFN),$(RUNFILTER), $(sort $^)); do \
	    CK_FORK=no $(VALGRIND) --leak-check=full -s $$j || exitCode=1; \
	done; \
	exit $$exitCode
else
ifeq ($(RUNMODE), coverage)
GCOV=$(shell which gcov)
ifeq ($(GCOV), )
$(error Couldn't find gcov in PATH. Please specify the path manually using GCOV param)
endif
# don't create gcov be default (anyway they ovewrite)
GCOVFLAGS=-n

# coverage test target
bin_test:
	@exitCode=0; \
	for j in $(call $(RUNFILTERFN),$(RUNFILTER),$(sort $^)); do \
	    $$j || exitCode=1; \
		$(GCOV) $(GCOVFLAGS) $$j; \
	done; \
	exit $$exitCode
else
ifneq ($(RUNMODE),)
$(error Unknown RUNMODE $(RUNMODE). Please check the parameter)
endif
# default test target
bin_test:
	@exitCode=0; \
	for j in $(call $(RUNFILTERFN),$(RUNFILTER),$(sort $^)); do \
	    $$j || exitCode=1; \
	done; \
	exit $$exitCode
endif
endif


test: bin_test
	@echo All test dependencies were: $^

.PHONY: bin_test test
