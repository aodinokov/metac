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
