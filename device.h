/* device.h
 *
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
