/* device.h
 *
 * Copyright (C) 2013 Austin Hendrix <namniart@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * CEC device interface for Python
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

#include <Python.h>

#include <libcec/cec.h>

struct Device {
   PyObject_HEAD

   CEC::cec_logical_address   addr;

   PyObject *                 vendorId;
   PyObject *                 physicalAddress;
   PyObject *                 cecVersion;
   PyObject *                 osdName;
   PyObject *                 lang;
};

PyTypeObject * DeviceTypeInit(CEC::ICECAdapter * adapter);

/*
 * Compat for libcec 3.x
 */
#if CEC_LIB_VERSION_MAJOR < 4
  #define CEC_MAX_DATA_PACKET_SIZE (16 * 4)
#endif
