#!/usr/bin/env python
# Callback test; just to see if callbacks are working.

from __future__ import print_function
from time import sleep
import cec

print("Loaded CEC from", cec.__file__)

def cb(event, *args):
    print("Got event", event, "with data", args)

# arguments: iils
def log_cb(event, level, time, message):
    print("CEC Log message:", message)

cec.add_callback(cb, cec.EVENT_ALL & ~cec.EVENT_LOG)
cec.add_callback(log_cb, cec.EVENT_LOG)
print("Callback added")
sleep(2)

if cec.HAVE_CEC_ADAPTER_DESCRIPTOR:
    print("CEC has cec_adapter_descriptor");
else:
    print("CEC does not have cec_adapter_descriptor");

print("Initializing CEC library")
cec.init()

print("Creating Device object for TV")
tv = cec.Device(0)
print("Turning on TV")
tv.power_on()


print("SUCCESS!")
