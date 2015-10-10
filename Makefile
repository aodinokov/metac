CFLAGS+=-g3 -o0

_always_:

%.dbg.o: %.c
	$(CC) -c $< -g3 $(CFLAGS) -o $@

%.ffi_meta.c: %.dbg.o
	nm $< | sed -re '/ffi_meta__/!d;s/.* ffi_meta__//;s/^([^_]*)_/\1 /' > $<.task
	dwarfdump $< | ./ffi_meta.awk -v file=$<.task > $@
	rm -f $<.task

ffi_meta_ut_001: -lcheck -ljansson -lffi
ffi_meta_ut_001: ffi_meta_ut_001.o ffi_meta_ut_001.ffi_meta.o ffi_meta.o

ffi_meta_ut_001.run_test: ffi_meta_ut_001 _always_
	./$^

all: ffi_meta_ut_001.ffi_meta.c ffi_meta_ut_001.run_test

clean:
	rm -rf *.o

.PHONY: all clean
