#! /usr/bin/env python
# $Id: setupmingw32.py,v 1.4 2020/04/06 10:20:38 nanard Exp $
# python script to build the miniupnpc module under windows
#
from distutils.core import setup, Extension
from distutils import sysconfig
sysconfig.get_config_vars()["OPT"] = ''
sysconfig.get_config_vars()["CFLAGS"] = ''
setup(name="libnatpmp", version="1.0",
      ext_modules=[
        Extension(name="libnatpmp", sources=["libnatpmpmodule.c"],
                  libraries=["ws2_32"],
                  extra_objects=["libnatpmp.a"],
                  define_macros=[('ENABLE_STRNATPMPERR', None)]
        )]
     )

