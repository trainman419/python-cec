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
   return Py_BuildValue("b", self->addr);
}

static PyObject * Device_getPhysicalAddress(Device * self,
      void * closure) {
   Py_INCREF(self->physicalAddress);
   return self->physicalAddress;
}

static PyObject * Device_getVendor(Device * self, void * closure) {
   Py_INCREF(self->vendorId);
   return self->vendorId;
}

static PyObject * Device_getOsdString(Device * self, void * closure) {
   Py_RETURN_NONE; // TODO
}

static PyObject * Device_getCECVersion(Device * self,
      void * closure) {
   Py_RETURN_NONE; // TODO
}

static PyObject * Device_getLanguage(Device * self, void * closure) {
   Py_RETURN_NONE; // TODO
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
   cec_power_status power = adapter->GetDevicePowerStatus(self->addr);
   PyObject * ret;
   switch(power) {
      case CEC_POWER_STATUS_ON:
      case CEC_POWER_STATUS_IN_TRANSITION_ON_TO_STANDBY:
         ret = Py_True;
         break;
      case CEC_POWER_STATUS_STANDBY:
      case CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON:
         ret = Py_False;
         break;
      case CEC_POWER_STATUS_UNKNOWN:
         PyErr_SetString(PyExc_IOError, "Power status not found");
         return NULL;
   }
   Py_INCREF(ret);
   return ret;
}

static PyObject * Device_power_on(Device * self) {
   if( adapter->PowerOnDevices(self->addr) ) {
      Py_RETURN_TRUE;
   } else {
      Py_RETURN_FALSE;
   }
}

static PyObject * Device_standby(Device * self) {
   if( adapter->StandbyDevices(self->addr) ) {
      Py_RETURN_TRUE;
   } else {
      Py_RETURN_FALSE;
   }
}

static PyObject * Device_new(PyTypeObject * type, PyObject * args, 
      PyObject * kwds) {
   Device * self;

   unsigned char addr;

   if( !PyArg_ParseTuple(args, "b:Device new", &addr) ) {
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
      self->addr = (cec_logical_address)addr;
      uint64_t vendor = adapter->GetDeviceVendorId(self->addr);
      if( ! (self->vendorId = Py_BuildValue("l", vendor)) ) return NULL;

      uint16_t physicalAddress = adapter->GetDevicePhysicalAddress(self->addr);
      char strAddr[8];
      snprintf(strAddr, 8, "%x.%x.%x.%x", 
            (physicalAddress >> 12) & 0xF,
            (physicalAddress >> 8) & 0xF,
            (physicalAddress >> 4) & 0xF,
            physicalAddress & 0xF);
      self->physicalAddress = Py_BuildValue("s", strAddr);

      cec_version ver = adapter->GetDeviceCecVersion(self->addr);
      // TODO: cec version to python type
      cec_osd_name name = adapter->GetDeviceOSDName(self->addr);
      // TODO: osd name to python type
      // TODO: get language and convert to python type
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
