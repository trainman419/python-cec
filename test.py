#!/usr/bin/env python

import cec

adapters = cec.list_adapters()

print adapters

if len(adapters) > 0:
   adapter = adapters[0]
   print "Using Adapter %s"%(adapter)
   cec.open()

   print "Success?!"
