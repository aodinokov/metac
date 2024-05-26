
all: test target

target:
	$(MAKE) -C ../../ M=$(PWD) target

clean:
	$(MAKE) -C ../../ M=$(PWD) clean

test:
	$(MAKE) -C ../../ M=$(PWD) test

.PHONY: all clean test

rules+= \
	target \
	_meta_c_app_complex \
	c_app_complex.meta.c \
	c_app_complex

TPL-target:=phony_target
IN-target:= \
	c_app_complex

in_c_app_complex+=c_app_complex.o module1f1.o module1f2.o module2.o module3.o

TPL-_meta_c_app_complex:=bin_target
IN-_meta_c_app_complex=$(in_c_app_complex:.o=.metac.o)
POST-_meta_c_app_complex=$(METAC_POST_META)
TPL-c_app_complex:=bin_target
IN-c_app_complex=$(in_c_app_complex) c_app_complex.meta.o

TPL-c_app_complex.meta.c:=metac_target
METACFLAGS-c_app_complex.meta.c+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-c_app_complex.meta.c=_meta_c_app_complex