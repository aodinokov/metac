CFLAGS+=-g3 -o0

all: ffi_meta_ut_001.ffi_meta.c ffi_meta_ut_001.run_test

_always_:

%.dbg.o: %.c
	$(CC) -c $< -g3 $(CFLAGS) -o $@

%.ffi_meta.c: %.dbg.o
	-nm --version
	nm $< | sed -re '/ffi_meta__/!d;s/.* ffi_meta__//;s/^([^_]*)_/\1 /' > $<.task
	@echo "task-------------------------------------------------------------------"
	@cat $<.task
	@echo "-----------------------------------------------------------------------"
	-awk --version
	-dwarfdump -V
	@echo "dwarf------------------------------------------------------------------"
	@dwarfdump $<
	@echo "-----------------------------------------------------------------------"
	dwarfdump $< | ./ffi_meta.awk -v file=$<.task > $@
	rm -f $<.task
	@echo "result-----------------------------------------------------------------"
	@cat $@
	@echo "-----------------------------------------------------------------------"

ffi_meta_ut_001: -lcheck
ffi_meta_ut_001: ffi_meta_ut_001.o ffi_meta_ut_001.ffi_meta.o ffi_meta.o

ffi_meta_ut_001.run_test: ffi_meta_ut_001 _always_
	./$^


clean:
	rm -rf *.o

.PHONY: all clean
