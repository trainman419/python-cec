all: cec.so

cec.so: build/lib.linux-x86_64-2.7/cec.so
	cp $< $@

build/lib.linux-x86_64-2.7/cec.so: cec.cpp
	python setup.py build
