# generates target to build phony targets based on non IN phony
define phony_target_tpl

$2: $$(addprefix $1/,$$(IN-$2))
	@echo Built dependencies: $^

.PHONY: $2

endef
phony_target = $(eval $(call phony_target_tpl,$1,$2))