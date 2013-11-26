# python-cec - libcec bindings for Python

`python-cec` allows you to control your TV, reciever and other CEC-compliant HDMI devices from a python script on a computer. Most computer graphics cards don't support CEC; you'll need a [Pulse-Eight USB-CEC adapter](http://www.pulse-eight.com/store/products/104-usb-hdmi-cec-adapter.aspx) or a Raspberry Pi (Untested).

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

Ubuntu:
```
sudo apt-get install libcec-dev build-essential python-dev
```
Note: because Ubuntu ships an older version of libcec that lacks the ToggleMute function, that function will also be missing in python-cec

### Checkout, build and install

```
git clone https://github.com/trainman419/python-cec.git
cd python-cec
python setup.py install
```

## Getting Started

A simple example to turn your TV on:

```python
import cec

cec.init()

tv = cec.Device(0)
tv.power_on()
```

## API


```python
import cec

adapters = cec.list_adapters() # may be called before init()

cec.init(adapter=cec.DEFAULT)

cec.close()  # not implemented yet

cec.add_callback(event, handler)  # not implemented yet

cec.remove_callback(event, handler)  # not implemented yet

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
   is_active() # not implemented yet
   set_av_input(input)
   set_audio_input(input)

cec.is_active_source() # not implemented yet
cec.set_active_source()  # not implemented yet
cec.set_inactive_source()  # not implemented yet

cec.volume_up()
cec.volume_down()
cec.toggle_mute()
# TODO: audio status

cec.set_physical_address(addr)
cec.can_persist_config()
cec.persist_config()
cec.set_port(device, port)
```

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
