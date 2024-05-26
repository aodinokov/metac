# generates target to build binaries and .so
define bin_target_tpl

# add extra LDFLAGS specific for this target
$$(addprefix $1/,$2): LDFLAGS+=$$(LDFLAGS-$2)

ifneq ($$(CC-$2),)
$$(addprefix $1/,$2): CC=$$(CC-$2)
endif

# rule to generate target from IN-files for this target. NOTE: manually add -shared ad LDFLAG for .so
$$(addprefix $1/,$2): $$(addprefix $1/,$$(IN-$2)) $$(DEPS-$2)
	@$$(PRE-$2)
	$$(CC) $$(filter %.o,$$^) $$(LDFLAGS) -o $$@
	$$(POST-$2)

# include extra dependencies generated during compilation of this target
-include $$(addprefix $1/,$$(IN-$2:.o=.d))

endef
bin_target = $(eval $(call bin_target_tpl,$1,$2))

# generates target to build binaries and .so
define bin_target_clean_tpl

# add .a, its .o and generated dep files to cleanup rule
clean: RMFLAGS+=$$(addprefix $1/,$2 $$(IN-$2) $$(IN-$2:.o=.d))

endef
bin_target_clean = $(eval $(call bin_target_clean_tpl,$1,$2))