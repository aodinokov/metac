CFLAGS+=-g3 -o0

all: metac_type_ut_001.run metac_type_ut_001.metac.c

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
	#@echo "result-----------------------------------------------------------------"
	#@cat $@
	#@echo "-----------------------------------------------------------------------"

# tests
metac_type_ut_001: -ldl -lcheck -lm -lrt -ljson
metac_type_ut_001: LDFLAGS=-pthread -rdynamic
metac_type_ut_001: metac_type.o metac_s11n_json.o metac_type_ut_001.o metac_type_ut_001.metac.o 

%.run: % _always_
	./$<

# documentation
doc: _always_
	@doxygen Doxyfile

clean:
	rm -rf *.o *.metac.c

.PHONY: all clean _always_
