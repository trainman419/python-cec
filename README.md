# python-cec - libcec bindings for Python

## Proposed API


```
import cec

adapters = cec.list_adapters()

cec.open( adapter = cec.DEFAULT )

cec.close()

cec.add_callback( event, handler )

cec.remove_callback( event, handler )

devices = cec.list_devices()

class Device:
   on()
   standby()
   address
   vendor
   osd_string
   cec_version
   power_status
   language
   name?
   #TODO: get volume
   #TODO: get active input

cec.set_active_source()
cec.set_inactive_source()

cec.volume_up()
cec.volume_down()

# TODO: set physical address
```
