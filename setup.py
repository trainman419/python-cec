#!/usr/bin/env python

from distutils.core import setup, Extension

python_cec = Extension('cec', sources = [ 'cec.cpp' ], libraries = [ 'cec' ])

setup(name='cec', version='0.1',
      description="Python bindings for libcec",
      ext_modules=[python_cec])
