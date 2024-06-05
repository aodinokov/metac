ifeq ($(M),)
METAC_ROOT=../..

all: c_print_args

c_print_args:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) $(PWD)/c_print_args

clean:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) clean

.PHONY: all clean
endif

CFLAGS+=-Wno-varargs

rules+=\
	c_print_args \
	_meta_c_print_args \
	main.reflect.c

TPL-c_print_args:=bin_target
IN-c_print_args=main.o main.reflect.o
LDFLAGS-c_print_args=-Lsrc -lmetac
DEPS-c_print_args=src/libmetac.a

TPL-_meta_c_print_args:=bin_target
IN-_meta_c_print_args=main.meta.o
LDFLAGS-_meta_c_print_args=-Lsrc -lmetac
DEPS-_meta_c_print_args=src/libmetac.a
POST-_meta_c_print_args=$(METAC_POST_META)

TPL-main.reflect.c:=metac_target
METACFLAGS-main.reflect.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-main.reflect.c=_meta_c_print_args

