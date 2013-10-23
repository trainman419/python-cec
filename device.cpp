/* device.cpp
 *
 * Implementation of CEC Device class for Python
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

#include "device.h"

using namespace CEC;

static ICECAdapter * adapter;

static PyObject * Device_getAddr(Device * self, void * closure) {
}

static PyObject * Device_getPhysicalAddress(Device * self,
      void * closure) {
}

static PyObject * Device_getVendor(Device * self, void * closure) {
}

static PyObject * Device_getOsdString(Device * self, void * closure) {
}

static PyObject * Device_getCECVersion(Device * self,
      void * closure) {
}

static PyObject * Device_getLanguage(Device * self, void * closure) {
}

static PyGetSetDef Device_getset[] = {
   {"address", (getter)Device_getAddr, (setter)NULL,
      "Logical Address"},
   {"physical_address", (getter)Device_getPhysicalAddress, (setter)NULL,
      "Physical Addresss"},
   {"vendor", (getter)Device_getVendor, (setter)NULL,
      "Vendor ID"},
   {"osd_string", (getter)Device_getOsdString, (setter)NULL,
      "OSD String"},
   {"cec_version", (getter)Device_getCECVersion, (setter)NULL,
      "CEC Version"},
   {"language", (getter)Device_getLanguage, (setter)NULL,
      "Language"},
   {NULL}
};

static PyObject * Device_is_on(Device * self) {
}

static PyObject * Device_power_on(Device * self) {
}

static PyObject * Device_standby(Device * self) {
}

static PyObject * Device_new(PyTypeObject * type, PyObject * args, 
      PyObject * kwds) {
   Device * self;

   int addr;

   if( !PyArg_ParseTuple(args, "i:Device new", &addr) ) {
      return NULL;
   }
   if( addr < 0 ) {
      PyErr_SetString(PyExc_ValueError, "Logical address should be >= 0");
      return NULL;
   }
   if( addr > 15 ) {
      PyErr_SetString(PyExc_ValueError, "Logical address should be < 16");
      return NULL;
   }

   self = (Device*)type->tp_alloc(type, 0);
   if( self != NULL ) {
      //self->addr = addr;
   }

   return (PyObject *)self;
}

static PyMethodDef Device_methods[] = {
   {"is_on", (PyCFunction)Device_is_on, METH_NOARGS, 
      "Get device power status"},
   {"power_on", (PyCFunction)Device_power_on, METH_NOARGS, 
      "Power on this device"},
   {"standby", (PyCFunction)Device_standby, METH_NOARGS, 
      "Put this device into standby"},
   {NULL}
};

static PyTypeObject DeviceType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /*ob_size*/
   "cec.Device",              /*tp_name*/
   sizeof(Device),            /*tp_basicsize*/
   0,                         /*tp_itemsize*/
   0,                         /*tp_dealloc*/
   0,                         /*tp_print*/
   0,                         /*tp_getattr*/
   0,                         /*tp_setattr*/
   0,                         /*tp_compare*/
   0,                         /*tp_repr*/
   0,                         /*tp_as_number*/
   0,                         /*tp_as_sequence*/
   0,                         /*tp_as_mapping*/
   0,                         /*tp_hash */
   0,                         /*tp_call*/
   0,                         /*tp_str*/
   0,                         /*tp_getattro*/
   0,                         /*tp_setattro*/
   0,                         /*tp_as_buffer*/
   Py_TPFLAGS_DEFAULT,        /*tp_flags*/
   "CEC Device objects",      /* tp_doc */
};

PyTypeObject * DeviceTypeInit(ICECAdapter * a) {
   adapter = a;
   DeviceType.tp_new = Device_new;
   DeviceType.tp_methods = Device_methods;
   DeviceType.tp_getset = Device_getset;
   return & DeviceType;
}
