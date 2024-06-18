ifeq ($(M),)
METAC_ROOT=../..

all: target test

target:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) $(PWD)/c_print_args

clean:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) clean

test:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) test

.PHONY: all clean
endif

CFLAGS+=-Wno-varargs -g3

rules+=\
	c_print_args \
	_meta_c_print_args \
	main.reflect.c

TPL-c_print_args:=bin_target
IN-c_print_args=print_args.o main.o main.reflect.o
LDFLAGS-c_print_args=-Lsrc -lmetac
DEPS-c_print_args=src/libmetac.a

TPL-_meta_c_print_args:=bin_target
IN-_meta_c_print_args=print_args.meta.o main.meta.o
LDFLAGS-_meta_c_print_args=-Lsrc -lmetac
DEPS-_meta_c_print_args=src/libmetac.a
POST-_meta_c_print_args=$(METAC_POST_META)

TPL-main.reflect.c:=metac_target
METACFLAGS-main.reflect.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-main.reflect.c=_meta_c_print_args

#link tests with libmetac
LDFLAGS-print_args_test=-Lsrc -lmetac
DEPS-print_args_test=src/libmetac.a
