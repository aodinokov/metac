FFI_CFLAGS=$(shell pkg-config --cflags libffi)
FFI_LDFLAGS=$(shell pkg-config --libs libffi)

ifeq ($(M),)
METAC_ROOT=../..

all:

#build only if ffi is installed
ifneq ($(FFI_LDFLAGS),)
all: target test

target:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) $(PWD)/c_ffi_call

test:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) test

else
target:

test:

endif

clean:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) clean

.PHONY: all clean
endif

CFLAGS+=-Wno-varargs -g3 $(FFI_CFLAGS)

rules+=\
	c_ffi_call \
	_meta_c_ffi_call \
	main.reflect.c

TPL-c_ffi_call:=bin_target
IN-c_ffi_call=main.o value_ffi.o main.reflect.o
LDFLAGS-c_ffi_call=-Lsrc -lmetac $(FFI_LDFLAGS)
DEPS-c_ffi_call=src/libmetac.a

TPL-_meta_c_ffi_call:=bin_target
IN-_meta_c_ffi_call=main.meta.o value_ffi.o
LDFLAGS-_meta_c_ffi_call=-Lsrc -lmetac $(FFI_LDFLAGS)
DEPS-_meta_c_ffi_call=src/libmetac.a
POST-_meta_c_ffi_call=$(METAC_POST_META)

TPL-main.reflect.c:=metac_target
METACFLAGS-main.reflect.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-main.reflect.c=_meta_c_ffi_call

#link tests with libmetac
LDFLAGS-value_ffi_test=-Lsrc -lmetac $(FFI_LDFLAGS)
DEPS-value_ffi_test=src/libmetac.a

