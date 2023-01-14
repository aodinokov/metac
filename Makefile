CFLAGS+=-g3 -o0 -D_GNU_SOURCE -Iinclude -Isrc

all:

_always_:

%.dwarfdump: %
	-dwarfdump -V
	dwarfdump $< > $@
	@#echo "dwarf------------------------------------------------------------------"
	@#cat $@
	@#echo "-----------------------------------------------------------------------"
.PRECIOUS: %.dwarfdump

%.metac.task: %
	-nm --version
	nm $< | sed -re '/metac__/!d;s/.* metac__//;s/^([^_]*)_/\1 /' > $@
	@echo "task-------------------------------------------------------------------"
	@cat $@
	@echo "-----------------------------------------------------------------------"
.PRECIOUS: %.metac.task

# this rule is only for linux
%.metac.c: %.dbg.o %.dbg.o.dwarfdump %.dbg.o.metac.task
	-awk --version
	cat $<.dwarfdump | ./bin/metac.awk -v file=$<.metac.task > $@
.PRECIOUS: %.metac.c
%.dbg.o: %.c
	$(CC) -c $< -g3 $(CFLAGS) -o $@

metac_objs = \
	src/type.o \
	src/refcounter.o \
	src/array_info.o 

metac_libs = -lm -lrt

libmetac.a: $(metac_objs)
	$(AR) $(ARFLAGS) $@ $^

# tests
metac_ut_libs = -ldl
metac_ut_libs += -lcheck
metac_ut_libs += -lsubunit
metac_ut_libs += $(metac_libs)

ut/metac_type_ut_001: LDFLAGS=-pthread -rdynamic
ut/metac_type_ut_001: ut/metac_type_ut_001.o ut/metac_type_ut_001.metac.o libmetac.a
ut/metac_type_ut_001: $(metac_ut_libs)

all: libmetac.a
all: ut/metac_type_ut_001.run 

%.run: % _always_
	#./$<
	which valgrind && LD_LIBRARY_PATH=/usr/local/lib/ CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$< || LD_LIBRARY_PATH=/usr/local/lib/ ./$< 
	#which valgrind && valgrind --trace-children=yes --leak-check=full --show-leak-kinds=all --track-origins=yes ./$< || ./$<

# documentation
doc: _always_
	@doxygen Doxyfile

clean:
	rm -rf src/*.o src/backends/*.o ut/*.o ut/*.metac.c *.a

.PHONY: all clean _always_
