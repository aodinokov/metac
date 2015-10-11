CFLAGS+=-g3 -o0

all: metac_ut_001.metac.c metac_ut_001.run_test

_always_:

%.dbg.o: %.c
	$(CC) -c $< -g3 $(CFLAGS) -o $@

%.metac.c: %.dbg.o
	-nm --version
	nm $< | sed -re '/metac__/!d;s/.* metac__//;s/^([^_]*)_/\1 /' > $<.task
	#@echo "task-------------------------------------------------------------------"
	#@cat $<.task
	#@echo "-----------------------------------------------------------------------"
	-awk --version
	-dwarfdump -V
	#@echo "dwarf------------------------------------------------------------------"
	#@dwarfdump $<
	#@echo "-----------------------------------------------------------------------"
	dwarfdump $< | ./metac.awk -v file=$<.task > $@
	rm -f $<.task
	@echo "result-----------------------------------------------------------------"
	@cat $@
	@echo "-----------------------------------------------------------------------"

metac_ut_001: -lcheck
metac_ut_001: metac_ut_001.o metac_ut_001.metac.o metac.o

metac_ut_001.run_test: metac_ut_001 _always_
	./$^


clean:
	rm -rf *.o *.metac.c

.PHONY: all clean
