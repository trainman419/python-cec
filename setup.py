#!/usr/bin/env python

from distutils.core import setup, Extension

# Remove the "-Wstrict-prototypes" compiler option, which isn't valid for C++.
import distutils.sysconfig
cfg_vars = distutils.sysconfig.get_config_vars()
if "CFLAGS" in cfg_vars:
    cfg_vars["CFLAGS"] = cfg_vars["CFLAGS"].replace("-Wstrict-prototypes", "")
if "OPT" in cfg_vars:
    cfg_vars["OPT"] = cfg_vars["OPT"].replace("-Wstrict-prototypes", "")

python_cec = Extension('cec', sources = [ 'cec.cpp', 'device.cpp' ], 
                        include_dirs=['include'],
                        libraries = [ 'cec' ])

setup(name='cec', version='0.2.8',
      description="Python bindings for libcec",
      long_description=open("README.md", "r", encoding="utf-8").read(),
      long_description_content_type="text/markdown",
      license='GPLv2',
      url="https://github.com/trainman419/python-cec",
      project_urls={
          "Bug Tracker": "https://github.com/trainman419/python-cec/issues",
      },
      author="Austin Hendrix",
      author_email="namniart@gmail.com",
      data_files=['COPYING'],
      ext_modules=[python_cec])
