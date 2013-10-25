#!/usr/bin/env python

import time
import cec

adapters = cec.list_adapters()

test_power = False

print adapters

if len(adapters) > 0:
   adapter = adapters[0]
   print "Using Adapter %s"%(adapter)
   cec.init()

   d = cec.Device(0)

   # print fields
   print "Address:", d.address
   print "Physical Address:", d.physical_address
   print "Vendor ID:", d.vendor
   print "OSD:", d.osd_string
   print "CEC Version:", d.cec_version
   print "Language:", d.language

   # call functions
   print "ON:", d.is_on()

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

   print "Success!"
