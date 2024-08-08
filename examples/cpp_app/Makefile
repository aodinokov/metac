
all: test target

target:
	$(MAKE) -C ../../ M=$(PWD) target

clean:
	$(MAKE) -C ../../ M=$(PWD) clean

test:
	$(MAKE) -C ../../ M=$(PWD) test

.PHONY: all clean test target

rules+= \
	target \
	cpp_app.reflect.cpp \
	cpp_app \
	_meta_cpp_app


TPL-target:=phony_target
IN-target:= \
	cpp_app

in-cpp_app=cpp_app.o cpp_app_class.o

TPL-_meta_cpp_app:=bin_target
CC-_meta_cpp_app=g++
IN-_meta_cpp_app=$(in-cpp_app:.o=.meta.o)
POST-_meta_cpp_app=$(METAC_POST_META)

TPL-cpp_app:=bin_target
CC-cpp_app=g++
IN-cpp_app=$(in-cpp_app) cpp_app.reflect.o

TPL-cpp_app.reflect.cpp:=metac_target
METACFLAGS-cpp_app.reflect.cpp+=run metac-reflect-gen $(METAC_OVERRIDE_IN_TYPE)
IN-cpp_app.reflect.cpp=_meta_cpp_app