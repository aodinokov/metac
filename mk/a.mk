# generates target to build .a
define a_target_tpl

# add extra ARFLAGS specific for this target
$$(addprefix $1/,$2): ARFLAGS+=$$(ARFLAGS-$2)

# rule to generate target .a from IN-files for this target
$$(addprefix $1/,$2): $$(addprefix $1/,$$(IN-$2))
	$$(AR) $$(ARFLAGS) $$@ $$^

# include extra dependencies generated during compilation of this target
-include $$(addprefix $1/,$$(IN-$2:.o=.d))

endef
a_target = $(eval $(call a_target_tpl,$1,$2))

# generates target to clean all .a artefacts
define a_target_clean_tpl
# add .a, its .o and generated dep files to cleanup rule
clean: RMFLAGS+=$$(addprefix $1/,$2 $$(IN-$2) $$(IN-$2:.o=.d))
endef
a_target_clean = $(eval $(call a_target_clean_tpl,$1,$2))