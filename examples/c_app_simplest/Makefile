ifeq ($(M),)
METAC_ROOT=../..

all: test target

target:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) target

clean:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) clean

test:
	$(MAKE) -C $(METAC_ROOT) M=$(PWD) test

.PHONY: all clean test
endif

rules+= \
	target \
	_meta_c_app \
	c_app.meta.c \
	c_app \

LDFLAGS-c_app=-Lsrc -lmetac
LDFLAGS-_meta_c_app=-Lsrc -lmetac

in_c_app+=main.o

TPL-_meta_c_app:=bin_target
IN-_meta_c_app=$(in_c_app:.o=.metac.o)
POST-_meta_c_app=$(METAC_POST_META)

TPL-c_app:=bin_target
IN-c_app=$(in_c_app) c_app.meta.o
DEPS-c_app=src/libmetac.a

TPL-c_app.meta.c:=metac_target
METACFLAGS-c_app.meta.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-c_app.meta.c=_meta_c_app

TPL-target:=phony_target
IN-target:= \
	c_app

