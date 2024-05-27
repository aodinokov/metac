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

rules+= \
	target \
	demodb

TPL-target:=phony_target
IN-target:= \
	demodb

TPL-demodb:=bin_target
IN-demodb= \
	main.o \
	demodb.o \
	demodb.reflect.o
LDFLAGS-demodb=-Lsrc -lmetac
DEPS-demodb=src/libmetac.a

#added to support demodb.reflect.o
rules+= \
	_meta_demodb \
	demodb.reflect.c

TPL-_meta_demodb=bin_target
IN-_meta_demodb= \
	main.meta.o \
	demodb.meta.o
POST-_meta_demodb=$(METAC_POST_META)

TPL-demodb.reflect.c:=metac_target
IN-demodb.reflect.c=_meta_demodb
METACFLAGS-demodb.reflect.c=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)

#link tests with libmetac
LDFLAGS-demodb_test=-Lsrc -lmetac
DEPS-demodb_test=src/libmetac.a