CFLAGS+=-g3 -o0

all: metac_type_ut_001.run_test metac_object_ut_001.run_test metac_type_ut_001.metac.c

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
metac_type_ut_001: -lcheck
metac_type_ut_001: metac_type_ut_001.o metac_type_ut_001.metac.o metac_type.o metac_object.o

metac_object_ut_001: -lcheck
metac_object_ut_001: metac_object_ut_001.o metac_object_ut_001.metac.o metac_type.o metac_object.o


%.run_test: % _always_
	./$^


clean:
	rm -rf *.o *.metac.c

.PHONY: all clean
