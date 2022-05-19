# python-cec - libcec bindings for Python

`python-cec` allows you to control your TV, reciever and other CEC-compliant HDMI devices from a python script on a computer. Most computer graphics cards don't support CEC; you'll need a [Pulse-Eight USB-CEC adapter](http://www.pulse-eight.com/store/products/104-usb-hdmi-cec-adapter.aspx) or a Raspberry Pi (tested on Raspberry Pi 3 Model B Rev 1.2).

## Installing:

### Install dependencies
To build python-cec, you need version 1.6.1 or later of the libcec development libraries:

On Gentoo:
```
sudo emerge libcec
```

On OS X:
```
brew install libcec
```

Ubuntu, Debian and Raspbian:
```
sudo apt-get install libcec-dev build-essential python-dev
```

### Install from PIP

```
pip install cec
```

### Installing on Windows

You need to [build libcec](https://github.com/Pulse-Eight/libcec/blob/master/docs/README.windows.md) from source, because libcec installer doesn't provide *cec.lib* that is necessary for linking.

Then you just need to set up your paths, e.g.:
```
set INCLUDE=path_to_libcec\build\amd64\include
set LIB=path_to_libcec\build\amd64
```

## Getting Started

A simple example to turn your TV on:

```python
import cec

cec.init()

tv = cec.Device(cec.CECDEVICE_TV)
tv.power_on()
```

## API


```python
import cec

adapters = cec.list_adapters() # may be called before init()

cec.init() # use default adapter
cec.init(adapter) # use a specific adapter

cec.close()  # not implemented yet

cec.add_callback(handler, events)
# the list of events is specified as a bitmask of the possible events:
cec.EVENT_LOG
cec.EVENT_KEYPRESS
cec.EVENT_COMMAND
cec.EVENT_CONFIG_CHANGE # not implemented yet
cec.EVENT_ALERT
cec.EVENT_MENU_CHANGED
cec.EVENT_ACTIVATED
cec.EVENT_ALL
# the callback will receive a varying number and type of arguments that are
# specific to the event. Contact me if you're interested in using specific
# callbacks

cec.remove_callback(handler, events)

devices = cec.list_devices()

class Device:
   __init__(id)
   is_on()
   power_on()
   standby()
   address
   physical_address
   vendor
   osd_string
   cec_version
   language
   is_active()
   set_av_input(input)
   set_audio_input(input)
   transmit(opcode, parameters)

cec.is_active_source(addr)
cec.set_active_source() # use default device type
cec.set_active_source(device_type) # use a specific device type
cec.set_inactive_source()  # not implemented yet

cec.volume_up()
cec.volume_down()
cec.toggle_mute()
# TODO: audio status

cec.set_physical_address(addr)
cec.can_persist_config()
cec.persist_config()
cec.set_port(device, port)

# set arbitrary active source (in this case 2.0.0.0)
destination = cec.CECDEVICE_BROADCAST
opcode = cec.CEC_OPCODE_ACTIVE_SOURCE
parameters = b'\x20\x00'
cec.transmit(destination, opcode, parameters)
```


## Environment Variables

CEC_OSD_NAME - by default this library will set the device name to "python-cec". If you set this 
environment varible (to a string <= 12 characters in length) your device will assigned that name
when the module initialises itself. This name will appear on your television when you scan for CEC devices.


## Changelog

### 0.2.8 ( 2022-01-05 )
* Add support for libCEC >= 5
* Windows support
* Support for setting CEC initiator
* Python 3.10 compatibility

### 0.2.7 ( 2018-11-09 )
* Implement cec.EVENT_COMMAND callback
* Fix several crashes/memory leaks related to callbacks
* Add possibility to use a method as a callback
* Limit maximum number of parameters passed to transmit()
* Fix compilation error with GCC >= 8

### 0.2.6 ( 2017-11-03 )
* Python 3 support ( @nforro )
* Implement is_active_source, set_active_source, transmit ( @nforro )
* libcec4 compatibility ( @nforro )

### 0.2.5 ( 2016-03-31 )
* re-release of version 0.2.4. Original release failed and version number is now lost

### 0.2.4 ( 2016-03-31 )
* libcec3 compatibility

### 0.2.3 ( 2014-12-28 )
* Add device.h to manifest
* Initial pip release

### 0.2.2 ( 2014-06-08 )
* Fix deadlock
* Add repr for Device

### 0.2.1 ( 2014-03-03 )
* Fix deadlock in Device

### 0.2.0 ( 2014-03-03 )
* Add initial callback implementation
* Fix libcec 1.6.0 backwards compatibility support

### 0.1.1 ( 2013-11-26 )
* Add libcec 1.6.0 backwards compatibility
* Known Bug: no longer compatible with libcec 2.1.0 and later

### 0.1.0 ( 2013-11-03 )
* First stable release

## Copyright

Copyright (C) 2013 Austin Hendrix <namniart@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
