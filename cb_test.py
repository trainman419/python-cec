#!/usr/bin/env python
# Callback test; just to see if callbacks are working.

from __future__ import print_function
from time import sleep
import cec

print("Loaded CEC from", cec.__file__)

def cb(event, data):
    print("Got event", event, "with data", data)

cec.add_callback(cb, cec.EVENT_ALL)
print("Callback added")
sleep(2)

cec.init()
print("CEC initialized. Waiting 10 seconds")

sleep(10)
