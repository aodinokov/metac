CFLAGS+=-g3 -o0 -D_GNU_SOURCE

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
	dwarfdump $< | ./metac.awk -v file=$<.task > $@
	rm -f $<.task
	@echo "result-----------------------------------------------------------------"
	#@cat $@
	@echo "-----------------------------------------------------------------------"

metac_objs = \
	metac_type.o \
	metac_internals.o \
	memory_backend_interface.o \
	memory_backend_pointer.o \
	memory_backend_json.o \
	traversing_engine.o

metac_ut_libs = -ldl -lcheck -lm -lrt -ljson
metac_ut_libs += -lsubunit

# tests
metac_type_ut_001: LDFLAGS=-pthread -rdynamic
metac_type_ut_001: $(metac_objs) metac_type_ut_001.o metac_type_ut_001.metac.o
metac_type_ut_001: $(metac_ut_libs)

metac_internals_ut_001: LDFLAGS=-pthread
metac_internals_ut_001: $(metac_objs) metac_internals_ut_001.o
metac_internals_ut_001: $(metac_ut_libs)


all: metac_type_ut_001.run metac_type_ut_001.metac.c
#all: metac_internals_ut_001.run

%.run: % _always_
	#./$<
	which valgrind && CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$< || ./$< 
	#which valgrind && valgrind --trace-children=yes --leak-check=full --show-leak-kinds=all --track-origins=yes ./$< || ./$<

# documentation
doc: _always_
	@doxygen Doxyfile

clean:
	rm -rf *.o *.metac.c

.PHONY: all clean _always_
