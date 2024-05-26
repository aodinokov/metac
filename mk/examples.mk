#find all examples and make rules for them
define examples_rules_tpl
example_$2_target: metac src/libmetac.a
	cd $1; $(MAKE) target
example_$2_test: metac src/libmetac.a
	cd $1; $(MAKE) test
example_$2_clean:
	cd $1; $(MAKE) clean
.PHONY: example_$2_all example_$2_test example_$2_clean
examples_target: example_$2_target
examples_test: example_$2_test
examples_clean: example_$2_clean
endef # examples_rules_tpl
examples_rules = $(eval $(call examples_rules_tpl,$1,$2))

examples: examples_test examples_target
.PHONY: examples examples_target examples_test examples_clean