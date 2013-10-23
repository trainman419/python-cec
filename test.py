#!/usr/bin/env python

import time
import cec

adapters = cec.list_adapters()

test_power = False

print adapters

if len(adapters) > 0:
   adapter = adapters[0]
   print "Using Adapter %s"%(adapter)
   cec.open()

   print "Success?!"

   d = cec.Device(0)
   print dir(d)

   # print fields
   print d.address
   print d.physical_address
   print d.vendor
   print d.osd_string
   print d.cec_version
   print d.language

   # call functions
   print d.is_on()

   if test_power:
      print "Powering device on"
      print d.power_on()
      print "Sleeping to allow device to power on"
      time.sleep(30)
      print d.is_on()
      
      print "Powering device off"
      print d.standby()
      print "Sleeping to allow device to power off"
      time.sleep(30)
      print d.is_on()
