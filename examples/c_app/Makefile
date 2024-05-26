ifeq ($(M),)
all: test target

target:
	$(MAKE) -C ../../ M=$(PWD) target

clean:
	$(MAKE) -C ../../ M=$(PWD) clean

test:
	$(MAKE) -C ../../ M=$(PWD) test

.PHONY: all clean test
endif

CFLAGS+=-DMETAC_MODULE_NAME="c_app" -g3

rules+= \
	target \
	_meta_c_app \
	c_app.meta.c \
	c_app \
	libc_app_static.a \
	libc_app.so


LDFLAGS-c_app=-Lsrc -lmetac
LDFLAGS-_meta_c_app=-Lsrc -lmetac
LDFLAGS-libc_app.so=-shared

in_c_app+=c_app_data.o c_app.o

TPL-_meta_c_app:=bin_target
IN-_meta_c_app=$(in_c_app:.o=.metac.o)
POST-_meta_c_app=$(METAC_POST_META)

TPL-c_app:=bin_target
IN-c_app=$(in_c_app) c_app.meta.o
DEPS-c_app=src/libmetac.a

TPL-c_app.meta.c:=metac_target
METACFLAGS-c_app.meta.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-c_app.meta.c=_meta_c_app

TPL-libc_app_static.a:=a_target
IN-libc_app_static.a+=c_app_data.o c_app.meta.o
TPL-libc_app.so:=bin_target
IN-libc_app.so+=c_app_data.o c_app.meta.o

TPL-target:=phony_target
IN-target:= \
	c_app \
	libc_app_static.a \
	libc_app.so

# tests
DEPS-c_app_data_test=src/libmetac.a
LDFLAGS-c_app_data_test=-Lsrc -lmetac
META-c_app_data_test=y
