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
	demodb.o

rules+= libdemodb.a
TPL-libdemodb.a:=a_target
IN-libdemodb.a=demodb.o

#tests
DEPS-demodb_alt_test:=$(M)/libdemodb.a
LDFLAGS-demodb_alt_test:=-L$(M) -ldemodb
REFLECT-demodb_alt_test:=n

REFLECT-demodb_test:=n