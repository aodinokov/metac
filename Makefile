CFLAGS+=-g3 -o0 -D_GNU_SOURCE -Iinclude -Isrc

all:

_always_:

%.dbg.o: %.c
	$(CC) -c $< -g3 $(CFLAGS) -o $@

%.metac.c: %.dbg.o
	-nm --version
	nm $< | sed -re '/metac__/!d;s/.* metac__//;s/^([^_]*)_/\1 /' > $<.task
	@echo "task-------------------------------------------------------------------"
	@cat $<.task
	@echo "-----------------------------------------------------------------------"
	-awk --version
	-dwarfdump -V
	#@echo "dwarf------------------------------------------------------------------"
	#@dwarfdump $< > xxx
	#@echo "-----------------------------------------------------------------------"
	dwarfdump $< | ./bin/metac.awk -v file=$<.task > $@
	rm -f $<.task
	@echo "result-----------------------------------------------------------------"
	#@cat $@
	@echo "-----------------------------------------------------------------------"

metac_objs = \
	src/metac_type.o \
	src/traversing_engine.o \
	src/metac_internals.o \
	src/memory_backend_interface.o \
	src/backends/memory_backend_pointer.o \
	src/backends/memory_backend_json.o

metac_libs = -lm -lrt -ljson-c

libmetac.a: $(metac_objs)
	$(AR) $(ARFLAGS) $@ $^

# tests
metac_ut_libs = $(metac_libs) -ldl
metac_ut_libs += -lsubunit
metac_ut_libs += -lcheck

ut/metac_type_ut_001: LDFLAGS=-pthread -rdynamic
ut/metac_type_ut_001: $(metac_objs) ut/metac_type_ut_001.o ut/metac_type_ut_001.metac.o libmetac.a
ut/metac_type_ut_001: $(metac_ut_libs)

ut/metac_internals_ut_001: LDFLAGS=-pthread
ut/metac_internals_ut_001: $(metac_objs) ut/metac_internals_ut_001.o
ut/metac_internals_ut_001: $(metac_ut_libs)


all: libmetac.a ut/metac_type_ut_001.run ut/metac_type_ut_001.metac.c
#all: metac_internals_ut_001.run

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
