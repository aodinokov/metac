ifeq ($(M),)
METAC_ROOT=../../..

all: test target

target:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) target

clean:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) clean

test:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) test

.PHONY: all clean test
endif

rules+=\
	target \
	param_demo \
	_meta_param_demo \
	main.reflect.c

TPL-target:=phony_target
IN-target:= \
	param_demo

TPL-param_demo:=bin_target
IN-param_demo=main.o main.reflect.o
LDFLAGS-param_demo=-Lsrc -lmetac
DEPS-param_demo=src/libmetac.a

TPL-_meta_param_demo:=bin_target
IN-_meta_param_demo=main.meta.o
LDFLAGS-_meta_param_demo=-Lsrc -lmetac
DEPS-_meta_param_demo=src/libmetac.a
POST-_meta_param_demo=$(METAC_POST_META)

TPL-main.reflect.c:=metac_target
METACFLAGS-main.reflect.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-main.reflect.c=_meta_param_demo