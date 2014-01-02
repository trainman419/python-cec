#!/usr/bin/env python
# Callback test; just to see if callbacks are working.

from __future__ import print_function
from time import sleep
import cec

def cb(event, data):
    print("Got event", event, "with data", data)

cec.add_callback(cec.EVENT_ALL, cb)
print("Callback added")
sleep(2)

cec.init()
print("CEC initialized. Waiting 10 seconds")

sleep(10)
