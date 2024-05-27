ifneq ($(DWARF_VER_FORCE),) #3,4 are currently supported on Ubuntu
METAC_DWARF_VER_CFLAGS=-gdwarf-$(DWARF_VER_FORCE)
endif
# o-file with disabled metac macroses and in debug mode to export DWARF 
# %.meta.o: CFLAGS+=-g3 -D_METAC_OFF_ - doesn't work when set CFLAGS as args
%.meta.o: %.c
	$(CC) $(CFLAGS) -g3 $(METAC_DWARF_VER_CFLAGS) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.meta.o: %.cpp
	$(CC) $(CFLAGS) -g3 $(METAC_DWARF_VER_CFLAGS) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.meta.o: %.cxx
	$(CC) $(CFLAGS) -g3 $(METAC_DWARF_VER_CFLAGS) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
# nometa - use those files if metainformation from that source must be ommitted in metadb
%.nometa.o: %.c
	$(CC) $(filter-out -g%,$(CFLAGS)) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.nometa.o: %.cpp
	$(CC) $(filter-out -g%,$(CFLAGS)) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<
%.nometa.o: %.cxx
	$(CC) $(filter-out -g%,$(CFLAGS)) -D_METAC_OFF_ -c -MMD -MF $(@:.o=.d) -MP -MT '$@ $(@:.o=.d)' -o $@ $<


#defaults (linux)
METAC_DWARFY_SRC=elf
METAC_PATH_CONV_CMD=$<
METAC_CHECK_CFLAGS=$(shell pkg-config --cflags check)
METAC_CHECK_LDFLAGS=$(shell pkg-config --libs check)

# OS-dependent part
ifeq ($(OS),Windows_NT)
	METAC_DWARFY_SRC=pe
	METAC_PATH_CONV_CMD=$(shell cygpath -m $<)
else 
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		# everything is default
	endif
	ifeq ($(UNAME_S),Darwin)
		METAC_DWARFY_SRC=macho
		METAC_POST_META=(which dsymutil) && dsymutil $@ || echo "Couldn't find dsymutil"
	endif
endif
METAC_OVERRIDE_IN_TYPE=-s 'path_type: "$(METAC_DWARFY_SRC)"'

# generates rules to build some files using metac
define metac_target_tpl
# generate c file from some binary using METAC
$$(addprefix $1/,$2): $$(addprefix $1/,$$(IN-$2)) $$(METAC)
	$$(METAC) $$(METACFLAGS-$2) -s 'path: "$$(METAC_PATH_CONV_CMD)"' > $$@
endef
metac_target=$(eval $(call metac_target_tpl,$1,$2))

# generates rules to clean files built using metac
define metac_target_clean_tpl
# list of generated files to cleanup
clean: RMFLAGS+=$$(addprefix $1/,$2)
endef
metac_target_clean=$(eval $(call metac_target_clean_tpl,$1,$2))
