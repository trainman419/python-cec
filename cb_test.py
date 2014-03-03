#!/usr/bin/env python
# Callback test; just to see if callbacks are working.

from __future__ import print_function
from time import sleep
import cec

print("Loaded CEC from", cec.__file__)

def cb(event, *args):
    print("Got event", event, "with data", args)

cec.add_callback(cb, cec.EVENT_ALL)
print("Callback added")
sleep(2)

if cec.HAVE_CEC_ADAPTER_DESCRIPTOR:
    print("CEC has cec_adapter_descriptor");
else:
    print("CEC does not have cec_adapter_descriptor");

cec.init()
print("CEC initialized. Waiting 10 seconds")

sleep(10)

print("SUCCESS!")
