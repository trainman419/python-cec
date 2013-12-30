/* cec.cpp
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
 * This file provides the python cec module, which is a python wrapper
 *  around libcec
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

#include <Python.h>
#include <stdlib.h>
#include <libcec/cec.h>
#include <list>

#include "device.h"

using namespace CEC;

// Basic design/usage:
//
// ( cec.add_callback(event, handler) )
//    ???
//
// ( cec.remove_callback(event, handler) )
//    ???
//
// ( cec.transmit( ??? ) )
//    call ICECAdapter::Transmit()
//
// Events:
//    - log message
//       message.message[1024]
//       message.level (bitfield)
//       message.time (int64)
//    - key press
//       key.keycode (int/enum)
//       key.duration (int)
//    - command
//       command.initiator (logical address)
//       command.destination (logical address)
//       command.ack
//       command.eom
//       command.opcode (enum)
//       command.parameters 
//       command.opcode_set (flag for when opcode is set; should probably use
//          this to set opcode to None)
//       command.transmit_timeout
//    - cec configuration changed (adapter/library config change)
//    - alert
//    - menu state changed
//       this is potentially thorny; see libcec source and CEC spec
//       ???
//    - source activated
//

#define EVENT_LOG           0x0001
#define EVENT_KEYPRESS      0x0002
#define EVENT_COMMAND       0x0004
#define EVENT_CONFIG_CHANGE 0x0008
#define EVENT_ALERT         0x0010
#define EVENT_MENU_CHANGED  0x0020
#define EVENT_ACTIVATED     0x0040
#define EVENT_ALL           0xFFFF

int parse_physical_addr(char * addr) {
   int a, b, c, d;
   if( sscanf(addr, "%x.%x.%x.%x", &a, &b, &c, &d) == 4 ) {
      if( a > 0xF || b > 0xF || c > 0xF || d > 0xF ) return -1;
      if( a < 0 || b < 0 || c < 0 || d < 0 ) return -1;
      return (a << 12) | (b << 8) | (c << 4) | d;
   } else {
      return -1;
   }
}

#define RETURN_BOOL(arg) do { PyObject * ret = (arg)?Py_True:Py_False; Py_INCREF(ret); return ret; } while(0)

void parse_test() {
   assert(parse_physical_addr("0.0.0.0") == 0);
   assert(parse_physical_addr("F.0.0.0") == 0xF000);
   assert(parse_physical_addr("0.F.0.0") == 0x0F00);
   assert(parse_physical_addr("0.0.F.0") == 0x00F0);
   assert(parse_physical_addr("0.0.0.F") == 0x000F);
   assert(parse_physical_addr("-1.0.0.0") == -1);
   assert(parse_physical_addr("0.-1.0.0") == -1);
   assert(parse_physical_addr("0.0.-1.0") == -1);
   assert(parse_physical_addr("0.0.0.-1") == -1);
   assert(parse_physical_addr("foo") == -1);
   assert(parse_physical_addr("F.F.F.F") == 0xFFFF);
   assert(parse_physical_addr("f.f.f.f") == 0xFFFF);
}

class CallbackList {
   private:
      std::list<PyObject*> callbacks;

   public:
      ~CallbackList() {
         // decrement the reference counts of everything in the list
         std::list<PyObject*>::const_iterator itr;
         for( itr = callbacks.begin(); itr != callbacks.end(); itr++ ) {
            Py_DECREF(*itr);
         }
      }

      void add(PyObject * cb) {
         assert(cb);
         Py_INCREF(cb);
         callbacks.push_back(cb);
      }

      PyObject * call(PyObject * args) {
         Py_INCREF(Py_None);
         PyObject * result = Py_None;

         std::list<PyObject*>::const_iterator itr;
         for( itr = callbacks.begin(); itr != callbacks.end(); itr++ ) {
            // see also: PyObject_CallFunction(...) which can take C args
            PyObject * temp = PyObject_CallObject(*itr, args);
            if( temp ) {
               Py_DECREF(temp);
            } else {
               Py_DECREF(Py_None);
               return NULL;
            }
         }
         return result;
      }
};

CallbackList * open_callbacks; // TODO: remove/update

ICECAdapter * CEC_adapter;
PyObject * Device;

std::list<cec_adapter> get_adapters() {
   std::list<cec_adapter> res;
   // release the Global Interpreter lock
   Py_BEGIN_ALLOW_THREADS
   // get adapters
   int cec_count = 10;
   cec_adapter * dev_list = (cec_adapter*)malloc(
         cec_count * sizeof(cec_adapter));
   int count = CEC_adapter->FindAdapters(dev_list, cec_count);
   if( count > cec_count ) {
      cec_count = count;
      dev_list = (cec_adapter*)realloc(dev_list, 
         cec_count * sizeof(cec_adapter));
      count = CEC_adapter->FindAdapters(dev_list, cec_count);
      count = std::min(count, cec_count);
   }

   for( int i=0; i<count; i++ ) {
      res.push_back(dev_list[i]);
   }

   free(dev_list);
   // acquire the GIL before returning to code that uses python objects
   Py_END_ALLOW_THREADS
   return res;
}

static PyObject * list_adapters(PyObject * self, PyObject * args) {
   PyObject * result = NULL;

   if( PyArg_ParseTuple(args, ":list_adapters") ) {
      std::list<cec_adapter> dev_list = get_adapters();
      // set up our result list
      result = PyList_New(0);

      // populate our result list
      std::list<cec_adapter>::const_iterator itr;
      for( itr = dev_list.begin(); itr != dev_list.end(); itr++ ) {
         PyList_Append(result, Py_BuildValue("s", itr->comm));
         /* Convert all of the fields 
         PyList_Append(result, Py_BuildValue("sshhhhii", 
                  itr->comm,
                  itr->strComPath,
                  itr->iVendorId,
                  itr->iProductId,
                  itr->iFirmwareVersion,
                  itr->iPhysicalAddress,
                  itr->iFirmwareBuildDate,
                  itr->adapterType
                  ));
                  */
      }
   }

   return result;
}

static PyObject * init(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   const char * dev = NULL;

   if( PyArg_ParseTuple(args, "|s:init", &dev) ) {
      if( !dev ) {
         std::list<cec_adapter> devs = get_adapters();
         if( devs.size() > 0 ) {
            dev = devs.front().comm;
         } else {
            PyErr_SetString(PyExc_Exception, "No default adapter found");
         }
      }
   }

   if( dev ) {
      bool success = false;
      Py_BEGIN_ALLOW_THREADS
      success = CEC_adapter->Open(dev);
      Py_END_ALLOW_THREADS
      if( success ) {
         Py_INCREF(Py_None);
         result = Py_None;
      } else {
         CECDestroy(CEC_adapter);
         CEC_adapter = NULL;

         char errstr[1024];
         snprintf(errstr, 1024, "CEC failed to open %s", dev);
         PyErr_SetString(PyExc_IOError, errstr);
      }
   }

   return result;
}

static PyObject * list_devices(PyObject * self, PyObject * args) {
   PyObject * result = NULL;

   if( PyArg_ParseTuple(args, ":list_devices") ) {
      cec_logical_addresses devices;
      Py_BEGIN_ALLOW_THREADS
      devices = CEC_adapter->GetActiveDevices();
      Py_END_ALLOW_THREADS

      //result = PyList_New(0);
      result = PyDict_New();
      for( uint8_t i=0; i<32; i++ ) {
         if( devices[i] ) {
            PyObject * ii = Py_BuildValue("(b)", i);
            PyObject * dev = PyObject_CallObject(Device, ii);
            Py_DECREF(ii);
            if( dev ) {
               //PyList_Append(result, dev); 
               PyDict_SetItem(result, Py_BuildValue("b", i), dev);
            } else {
               Py_DECREF(result);
               result = NULL;
               break;
            }
         }
      }
   }

   return result;
}

static PyObject * add_callback(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   long int event;
   PyObject * callback;

   if( PyArg_ParseTuple(args, "iO:add_callback", &event, &callback) ) {
      // TODO: check that event is one of the allowed events
      //  event should probably be a bitmask
      if( false ) {
         PyErr_SetString(PyExc_TypeError, "Invalid event for callback");
         return NULL;
      }
      if( !PyCallable_Check(callback)) {
         PyErr_SetString(PyExc_TypeError, "parameter must be callable");
         return NULL;
      }

      if( event & EVENT_LOG ) {
      }
      if( event & EVENT_KEYPRESS ) {
      }
      if( event & EVENT_COMMAND ) {
      }
      if( event & EVENT_CONFIG_CHANGE ) {
      }
      if( event & EVENT_ALERT ) {
      }
      if( event & EVENT_MENU_CHANGED ) {
      }
      if( event & EVENT_ACTIVATED ) {
      }
      open_callbacks->add(callback);
      Py_INCREF(Py_None);
      result = Py_None;
   }
   return result;
}

static PyObject * remove_callback(PyObject * sefl, PyObject * args) {
  if( PyArg_ParseTuple(args, "O:remove_callback") ) {
  }
  return NULL;
}

static PyObject * volume_up(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":volume_up") )
      RETURN_BOOL(CEC_adapter->VolumeUp());
   return NULL;
}

static PyObject * volume_down(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":volume_up") )
      RETURN_BOOL(CEC_adapter->VolumeDown());
   return NULL;
}

#if CEC_LIB_VERSION_MAJOR > 1
static PyObject * toggle_mute(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":toggle_mute") )
      RETURN_BOOL(CEC_adapter->AudioToggleMute());
   return NULL;
}
#endif

static PyObject * set_stream_path(PyObject * self, PyObject * args) {
   PyObject * arg;

   if( PyArg_ParseTuple(args, "O:set_stream_path", &arg) ) {
      Py_INCREF(arg);
      if(PyInt_Check(arg)) {
         long arg_l = PyInt_AsLong(arg);
         Py_DECREF(arg);
         if( arg_l < 0 || arg_l > 15 ) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
         } else {
            RETURN_BOOL(CEC_adapter->SetStreamPath((cec_logical_address)arg_l));
         }
      } else if(PyString_Check(arg)) {
         char * arg_s = PyString_AsString(arg);
         if( arg_s ) {
            int pa = parse_physical_addr(arg_s);
            Py_DECREF(arg);
            if( pa < 0 ) {
               PyErr_SetString(PyExc_ValueError, "Invalid physical address");
               return NULL;
            } else {
               RETURN_BOOL(CEC_adapter->SetStreamPath((uint16_t)pa));
            }
         } else {
            Py_DECREF(arg);
            return NULL;
         }
      } else {
         PyErr_SetString(PyExc_TypeError, "parameter must be string or int");
         return NULL;
      }
   }

   return NULL;
}

PyObject * set_physical_addr(PyObject * self, PyObject * args) {
   char * addr_s;
   if( PyArg_ParseTuple(args, "s:set_physical_addr", &addr_s) ) {
      int addr = parse_physical_addr(addr_s);
      if( addr >= 0 ) {
         RETURN_BOOL(CEC_adapter->SetPhysicalAddress((uint16_t)addr));
      } else {
         PyErr_SetString(PyExc_ValueError, "Invalid physical address");
         return NULL;
      }
   }
   return NULL;
}

PyObject * set_port(PyObject * self, PyObject * args) {
   unsigned char dev, port;
   if( PyArg_ParseTuple(args, "bb", &dev, &port) ) {
      if( dev > 15 ) {
         PyErr_SetString(PyExc_ValueError, "Invalid logical address");
         return NULL;
      }
      if( port > 15 ) {
         PyErr_SetString(PyExc_ValueError, "Invalid port");
         return NULL;
      }
      RETURN_BOOL(CEC_adapter->SetHDMIPort((cec_logical_address)dev, port));
   }
   return NULL;
}

PyObject * can_persist_config(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":can_persist_config") ) {
      RETURN_BOOL(CEC_adapter->CanPersistConfiguration());
   }
   return NULL;
}

PyObject * persist_config(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":persist_config") ) {
      if( ! CEC_adapter->CanPersistConfiguration() ) {
         PyErr_SetString(PyExc_NotImplementedError,
               "Cannot persist configuration");
         return NULL;
      }
      libcec_configuration config;
      if( ! CEC_adapter->GetCurrentConfiguration(&config) ) {
         PyErr_SetString(PyExc_IOError, "Could not get configuration");
         return NULL;
      }
      RETURN_BOOL(CEC_adapter->PersistConfiguration(&config));
   }
   return NULL;
}

static PyMethodDef CecMethods[] = {
   {"list_adapters", list_adapters, METH_VARARGS, "List available adapters"},
   {"init", init, METH_VARARGS, "Open an adapter"},
   {"list_devices", list_devices, METH_VARARGS, "List devices"},
   {"add_callback", add_callback, METH_VARARGS, "Add a callback"},
   {"remove_callback", remove_callback, METH_VARARGS, "Remove a callback"},
   {"volume_up",   volume_up,   METH_VARARGS, "Volume Up"},
   {"volume_down", volume_down, METH_VARARGS, "Volume Down"},
#if CEC_LIB_VERSION_MAJOR > 1
   {"toggle_mute", toggle_mute, METH_VARARGS, "Toggle Mute"},
#endif
   {"set_stream_path", set_stream_path, METH_VARARGS, "Set HDMI stream path"},
   {"set_physical_addr", set_physical_addr, METH_VARARGS,
      "Set HDMI physical address"},
   {"can_persist_config", can_persist_config, METH_VARARGS,
      "return true if the current adapter can persist the CEC configuration"},
   {"persist_config", persist_config, METH_VARARGS,
      "persist CEC configuration to adapter"},
   {"set_port", set_port, METH_VARARGS, "Set upstream HDMI port"},
   {NULL, NULL, 0, NULL}
};

libcec_configuration * CEC_config;
ICECCallbacks * CEC_callbacks; 

PyMODINIT_FUNC initcec(void) {
   // set up callback data structures
   open_callbacks = new CallbackList();

   // set up libcec
   //  libcec config
   CEC_config = new libcec_configuration();
   CEC_config->Clear();

   snprintf(CEC_config->strDeviceName, 13, "python-cec");
   CEC_config->clientVersion = CEC_CLIENT_VERSION_1_6_0;
   CEC_config->bActivateSource = 0;
   CEC_config->deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);


   //  libcec callbacks
   CEC_callbacks = new ICECCallbacks();
#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 7 )
   CEC_callbacks->Clear();
#endif

   CEC_config->callbacks = CEC_callbacks;

   CEC_adapter = (ICECAdapter*)CECInitialise(CEC_config);

   if( !CEC_adapter ) {
      PyErr_SetString(PyExc_IOError, "Failed to initialize libcec");
      return;
   }

#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 8 )
   CEC_adapter->InitVideoStandalone();
#endif

   // set up python module
   PyTypeObject * dev = DeviceTypeInit(CEC_adapter);
   Device = (PyObject*)dev;
   if(PyType_Ready(dev) < 0 ) return;

   // TODO: add constants for event types
   PyObject * m = Py_InitModule("cec", CecMethods);

   if( m == NULL ) return;

   Py_INCREF(dev);
   PyModule_AddObject(m, "Device", (PyObject*)dev);
   PyModule_AddIntMacro(m, EVENT_LOG);
   PyModule_AddIntMacro(m, EVENT_KEYPRESS);
   PyModule_AddIntMacro(m, EVENT_COMMAND);
   PyModule_AddIntMacro(m, EVENT_CONFIG_CHANGE);
   PyModule_AddIntMacro(m, EVENT_ALERT);
   PyModule_AddIntMacro(m, EVENT_MENU_CHANGED);
   PyModule_AddIntMacro(m, EVENT_ACTIVATED);
   PyModule_AddIntMacro(m, EVENT_ALL);
}
