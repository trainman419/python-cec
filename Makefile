PYTHON?=python

ARCH:=$(shell uname -m)
SYS:=$(shell uname -s | tr '[:upper:]' '[:lower:]')
PY_VER:=$(shell $(PYTHON) -c 'import sys; print("%d.%d"%(sys.version_info.major, sys.version_info.minor));')

SOABI:=$(shell $(PYTHON) -c 'import sysconfig; print(sysconfig.get_config_var("SOABI") or "")')
ifeq (,$(SOABI))
	EXTENSION:=cec.so
else
	EXTENSION:=cec.$(SOABI).so
endif

BUILD_DIR:=build/lib.$(SYS)-$(ARCH)-$(PY_VER)

all: $(EXTENSION)

$(EXTENSION): $(BUILD_DIR)/$(EXTENSION)
	cp $< $@

$(BUILD_DIR)/$(EXTENSION): cec.cpp setup.py device.h device.cpp
	$(PYTHON) setup.py build

dist: all
	$(PYTHON) setup.py bdist_wheel

test: all
	./test.py
.PHONY: test

clean:
	rm -rf build
	rm -f $(EXTENSION)
	rm -rf dist
	rm -rf cec.egg-info
	rm -f cec.*.so
.PHONY: clean
