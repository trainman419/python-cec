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
   Py_INCREF(self->osdName);
   return self->osdName;
}

static PyObject * Device_getCECVersion(Device * self,
      void * closure) {
   Py_INCREF(self->cecVersion);
   return self->cecVersion;
}

static PyObject * Device_getLanguage(Device * self, void * closure) {
   Py_INCREF(self->lang);
   return self->lang;
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
      char vendor_str[7];
      snprintf(vendor_str, 7, "%06lX", vendor);
      if( ! (self->vendorId = Py_BuildValue("s", vendor_str)) ) return NULL;

      uint16_t physicalAddress = adapter->GetDevicePhysicalAddress(self->addr);
      char strAddr[8];
      snprintf(strAddr, 8, "%x.%x.%x.%x", 
            (physicalAddress >> 12) & 0xF,
            (physicalAddress >> 8) & 0xF,
            (physicalAddress >> 4) & 0xF,
            physicalAddress & 0xF);
      self->physicalAddress = Py_BuildValue("s", strAddr);

      cec_version ver = adapter->GetDeviceCecVersion(self->addr);
      const char * ver_str;
      switch(ver) {
         case CEC_VERSION_1_2:
            ver_str = "1.2";
            break;
         case CEC_VERSION_1_2A:
            ver_str = "1.2a";
            break;
         case CEC_VERSION_1_3:
            ver_str = "1.3";
            break;
         case CEC_VERSION_1_3A:
            ver_str = "1.3a";
            break;
         case CEC_VERSION_1_4:
            ver_str = "1.4";
            break;
         case CEC_VERSION_UNKNOWN:
         default:
            ver_str = "Unknown";
            break;
      }

      if( !(self->cecVersion = Py_BuildValue("s", ver_str)) ) return NULL;

      cec_osd_name name = adapter->GetDeviceOSDName(self->addr);
      if( !(self->osdName = Py_BuildValue("s", name.name)) ) return NULL;

      cec_menu_language lang;
      adapter->GetDeviceMenuLanguage(self->addr, &lang);
      if( !(self->lang = Py_BuildValue("s", lang.language)) ) return NULL;
   }

   return (PyObject *)self;
}

static void Device_dealloc(Device * self) {
   Py_DECREF(self->vendorId);
   Py_DECREF(self->physicalAddress);
   Py_DECREF(self->cecVersion);
   Py_DECREF(self->osdName);
   Py_DECREF(self->lang);
   self->ob_type->tp_free((PyObject*)self);
}

static PyObject * Device_str(Device * self) {
   char addr[16];
   snprintf(addr, 16, "CEC Device %d", self->addr);
   return Py_BuildValue("s", addr);
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
   (destructor)Device_dealloc, /*tp_dealloc*/
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
   (reprfunc)Device_str,      /*tp_str*/
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
