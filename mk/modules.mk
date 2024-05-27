#template to test /modules
define modules_test_tpl

ifneq (,$$(filter %.c.yaml,$2))
$$(addprefix $1/,$(2:c.yaml=reflect.c)): $$(METAC) $$(addprefix $1/,$2)
	path=$$$$(pwd); (cd $1 && $$$${path}/$$(METAC) -f $$(subst $1/,,$$(filter %.yaml,$$^))) >$$@
	$$(CC) $$(CFLAGS) -c $$@ -o $$@.o

module_test: $$(addprefix $1/,$(2:.c.yaml=.reflect.c))
endif

endef # modules_test_tpl
modules_test = $(eval $(call modules_test_tpl,$1,$2))

define modules_test_clean_tpl
ifneq (,$$(filter %.c.yaml,$2))
clean: RMFLAGS+=$$(addprefix $1/,$(2:.c.yaml=.reflect.c) $(2:.c.yaml=.reflect.c.o))
endif
endef # modules_test_clean_tpl
modules_test_clean = $(eval $(call modules_test_clean_tpl,$1,$2))

test: module_test

.PHONY: module_test