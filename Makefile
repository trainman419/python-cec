all: cec.so

ARCH:=$(shell uname -m)
SYS:=$(shell uname -s | tr '[:upper:]' '[:lower:]')
PY_VER:=$(shell python -c 'import sys; print "%d.%d"%(sys.version_info.major, sys.version_info.minor);')

BUILD_DIR:=build/lib.$(SYS)-$(ARCH)-$(PY_VER)

cec.so: $(BUILD_DIR)/cec.so
	cp $< $@

$(BUILD_DIR)/cec.so: cec.cpp setup.py device.h device.cpp
	python setup.py build

test: all
	./test.py
.PHONY: test
