# python-cec - libcec bindings for Python

## Proposed API

```
class cec_configuration:
   ALL THE THINGS :(

class ICEAdapter:
   def open(str port, int timeout_ms=10000)
      throws error on failure

   def close()
      
   def ping()
      returns bool

   def transmit(cec_command data)
      throws error on failure

   def powerOnDevices(cec_logical_address address = CecTV)
      throws error on failure

   def standByDevices(cec_logical_address address = CecBroadcast)
      throws error on failure

   def setActiveSource(cec_device_type type = CecDeviceTypeReserved)
      throws error on failure

   def getDevicePowerStatus(cec_logical_address address)
      throws error on device not found
      returns cec_power_status
```
