all: cec.so

cec.so: build/lib.linux-x86_64-2.7/cec.so
#	cp $< $@

build/lib.linux-x86_64-2.7/cec.so: cec.cpp setup.py device.h device.cpp
	python setup.py build

test: all
	./test.py
.PHONY: test
