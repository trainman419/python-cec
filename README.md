# python-cec - libcec bindings for Python

## Proposed API


```
import cec

adapters = cec.list_adapters()

cec.open( adapter = cec.DEFAULT )

cec.close() // not implemented yet

cec.add_callback( event, handler ) // not implemented yet

cec.remove_callback( event, handler ) // not implemented yet

devices = cec.list_devices()

class Device:
   is_on()
   power_on()
   standby()
   address
   physical_address
   vendor
   osd_string // not implemented yet
   cec_version // not implemented yet
   language // not implemented yet
   #TODO: get volume
   #TODO: get active input

cec.set_active_source() // not implemented yet
cec.set_inactive_source() // not implemented yet

cec.volume_up()
cec.volume_down()

# TODO: set physical address
```
