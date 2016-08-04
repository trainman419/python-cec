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

// request the std format macros
#define __STDC_FORMAT_MACROS

#include <Python.h>
#include <stdlib.h>
#include <inttypes.h>
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
#define EVENT_VALID         0x007F
#define EVENT_ALL           0x007F

//#define DEBUG 1

#ifdef DEBUG
# define debug(...) printf("CEC DEBUG: " __VA_ARGS__)
#else
# define debug(...)
#endif

// cec_adapter_descriptor and DetectAdapters were introduced in 2.1.0
#if CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MINOR >= 1
#define CEC_ADAPTER_TYPE cec_adapter_descriptor
#define CEC_FIND_ADAPTERS DetectAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 1
#else
#define CEC_ADAPTER_TYPE cec_adapter
#define CEC_FIND_ADAPTERS FindAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 0
#endif

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

#define RETURN_BOOL(arg) do { \
  bool result; \
  Py_BEGIN_ALLOW_THREADS \
  result = (arg); \
  Py_END_ALLOW_THREADS \
  PyObject * ret = (result)?Py_True:Py_False; \
  Py_INCREF(ret); \
  return ret; \
} while(0)

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

ICECAdapter * CEC_adapter;
PyObject * Device;

std::list<CEC_ADAPTER_TYPE> get_adapters() {
   std::list<CEC_ADAPTER_TYPE> res;
   // release the Global Interpreter lock
   Py_BEGIN_ALLOW_THREADS
   // get adapters
   int cec_count = 10;
   CEC_ADAPTER_TYPE * dev_list = (CEC_ADAPTER_TYPE*)malloc(
         cec_count * sizeof(CEC_ADAPTER_TYPE));
   int count = CEC_adapter->CEC_FIND_ADAPTERS(dev_list, cec_count);
   if( count > cec_count ) {
      cec_count = count;
      dev_list = (CEC_ADAPTER_TYPE*)realloc(dev_list, 
         cec_count * sizeof(CEC_ADAPTER_TYPE));
      count = CEC_adapter->CEC_FIND_ADAPTERS(dev_list, cec_count);
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
      std::list<CEC_ADAPTER_TYPE> dev_list = get_adapters();
      // set up our result list
      result = PyList_New(0);

      // populate our result list
      std::list<CEC_ADAPTER_TYPE>::const_iterator itr;
      for( itr = dev_list.begin(); itr != dev_list.end(); itr++ ) {
#if HAVE_CEC_ADAPTER_DESCRIPTOR
         PyList_Append(result, Py_BuildValue("s", itr->strComName));
         /* Convert all of the fields 
         PyList_Append(result, Py_BuildValue("sshhhhii", 
                  itr->strComName,
                  itr->strComPath,
                  itr->iVendorId,
                  itr->iProductId,
                  itr->iFirmwareVersion,
                  itr->iPhysicalAddress,
                  itr->iFirmwareBuildDate,
                  itr->adapterType
                  ));
                  */
#else
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
#endif
      }
   }

   return result;
}

static PyObject * init(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   const char * dev = NULL;

   if( PyArg_ParseTuple(args, "|s:init", &dev) ) {
      if( !dev ) {
         std::list<CEC_ADAPTER_TYPE> devs = get_adapters();
         if( devs.size() > 0 ) {
#if HAVE_CEC_ADAPTER_DESCRIPTOR
            dev = devs.front().strComName;
#else
            dev = devs.front().comm;
#endif
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

class Callback {
   public:
      long int event;
      PyObject * cb;

      Callback(long int e, PyObject * c) : event(e), cb(c) {
         Py_INCREF(cb);
      }
      ~Callback() {
         Py_DECREF(cb);
      }
};


typedef std::list<Callback> cb_list;
cb_list callbacks;

static PyObject * add_callback(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   PyObject * callback;
   long int events = EVENT_ALL; // default to all events

   if( PyArg_ParseTuple(args, "O|i:add_callback", &callback, &events) ) {
      // check that event is one of the allowed events
      if( events & ~(EVENT_VALID) ) {
         PyErr_SetString(PyExc_TypeError, "Invalid event(s) for callback");
         return NULL;
      }
      if( !PyCallable_Check(callback)) {
         PyErr_SetString(PyExc_TypeError, "parameter must be callable");
         return NULL;
      }

      Callback new_cb(events, callback);

      debug("Adding callback for event %ld\n", events);
      callbacks.push_back(new_cb);

      Py_INCREF(Py_None);
      result = Py_None;
   }
   return result;
}

static PyObject * remove_callback(PyObject * self, PyObject * args) {
  PyObject * callback;
  long int events = EVENT_ALL; // default to all events

  if( PyArg_ParseTuple(args, "O|i:remove_callback", &callback, &events) ) {
     for( cb_list::iterator itr = callbacks.begin(); 
           itr != callbacks.end();
           ++itr ) {
        if( itr->cb == callback ) {
           // clear out the given events for this callback
           itr->event &= ~(events);
           if( itr->event == 0 ) {
              // if this callback has no events, remove it
              itr = callbacks.erase(itr);
           }
        }
     }
     
  }
  return NULL;
}

static PyObject * trigger_event(long int event, PyObject * args) {
   assert(event & EVENT_ALL);
   Py_INCREF(Py_None);
   PyObject * result = Py_None;

   //debug("Triggering event %ld\n", event);

   int i=0;
   for( cb_list::const_iterator itr = callbacks.begin();
         itr != callbacks.end();
         ++itr ) {
      //debug("Checking callback %d with events %ld\n", i, itr->event);
      if( itr->event & event ) {
         //debug("Calling callback %d\n", i);
         // see also: PyObject_CallFunction(...) which can take C args
         PyObject * temp = PyObject_CallObject(itr->cb, args);
         if( temp ) {
            debug("Callback succeeded\n");
            Py_DECREF(temp);
         } else {
            debug("Callback failed\n");
            Py_DECREF(Py_None);
            return NULL;
         }
      }
      i++;
   }

   return result;
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

static PyObject * set_active_source(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":set_active_source") )
      RETURN_BOOL(CEC_adapter->SetActiveSource());
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
      if(PyLong_Check(arg)) {
         long arg_l = PyLong_AsLong(arg);
         Py_DECREF(arg);
         if( arg_l < 0 || arg_l > 15 ) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
         } else {
            RETURN_BOOL(CEC_adapter->SetStreamPath((cec_logical_address)arg_l));
         }
#if PY_MAJOR_VERSION < 3
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
#endif
      } else if(PyUnicode_Check(arg)) {
         // Convert from Unicode to ASCII
         PyObject* ascii_arg = PyUnicode_AsASCIIString(arg);
         if (NULL == ascii_arg) {
            // Means the string can't be converted to ASCII, the codec failed
            PyErr_SetString(PyExc_ValueError,
               "Could not convert address to ASCII");
            return NULL;
         }

         // Get the actual bytes as a C string
         char * arg_s = PyByteArray_AsString(ascii_arg);
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
   {"set_active_source", set_active_source, METH_VARARGS, "Set Active Source"},
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

int log_cb(void * self, const cec_log_message message) {
   debug("got log callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   int level = message.level;
   long int time = message.time;
   PyObject * args = Py_BuildValue("(iils)", EVENT_LOG, 
         level,
         time,
         message.message);
   trigger_event(EVENT_LOG, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

int keypress_cb(void * self, const cec_keypress key) {
   debug("got keypress callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   PyObject * args = Py_BuildValue("(iBI)", EVENT_KEYPRESS,
         key.keycode,
         key.duration);
   trigger_event(EVENT_KEYPRESS, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

int command_cb(void * self, const cec_command command) {
   debug("got command callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   // TODO: figure out how to pass these parameters
   //  we'll probably have to build an Object for this
   PyObject * args = Py_BuildValue("(i)", EVENT_COMMAND);
   // don't bother triggering an event until we can actually pass arguments
   //trigger_event(EVENT_COMMAND, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

int config_cb(void * self, const libcec_configuration) {
   debug("got config callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   // TODO: figure out how to pass these as parameters
   // yeah... right. 
   //  we'll probably have to come up with some functions for converting the 
   //  libcec_configuration class into a python Object
   //  this will probably be _lots_ of work and should probably wait until
   //  a later release, or when it becomes necessary.
   PyObject * args = Py_BuildValue("(i)", EVENT_CONFIG_CHANGE);
   // don't bother triggering an event until we can actually pass arguments
   //trigger_event(EVENT_CONFIG_CHANGE, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

int alert_cb(void * self, const libcec_alert alert, const libcec_parameter p) {
   debug("got alert callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   PyObject * param = Py_None;
   if( p.paramType == CEC_PARAMETER_TYPE_STRING ) {
      param = Py_BuildValue("s", p.paramData);
   } else {
      Py_INCREF(param);
   }
   PyObject * args = Py_BuildValue("(iiN)", EVENT_ALERT, alert, param);
   trigger_event(EVENT_ALERT, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

int menu_cb(void * self, const cec_menu_state menu) {
   debug("got menu callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   PyObject * args = Py_BuildValue("(ii)", EVENT_MENU_CHANGED, menu);
   trigger_event(EVENT_MENU_CHANGED, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return 1;
}

void activated_cb(void * self, const cec_logical_address logical_address,
      const uint8_t state) {
   debug("got activated callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   PyObject * active = (state == 1) ? Py_True : Py_False;
   PyObject * args = Py_BuildValue("(iOi)", EVENT_ACTIVATED, active,
      logical_address);
   trigger_event(EVENT_ACTIVATED, args);
   Py_DECREF(args);
   PyGILState_Release(gstate);
   return;
}

PyMODINIT_FUNC initcec(void) {
   // Make sure threads are enabled in the python interpreter
   // this also acquires the global interpreter lock
   PyEval_InitThreads();

   // set up libcec
   //  libcec config
   CEC_config = new libcec_configuration();
   CEC_config->Clear();

   snprintf(CEC_config->strDeviceName, 13, "python-cec");
   // CEC_CLIENT_VERSION_CURRENT was introduced in 2.0.4
   // just use 2.1.0 because the conditional is simpler
#if CEC_LIB_VERSION_MAJOR >= 3
   CEC_config->clientVersion = LIBCEC_VERSION_CURRENT;
#elif CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MINOR >= 1
   CEC_config->clientVersion = CEC_CLIENT_VERSION_CURRENT;
#else
   // fall back to 1.6.0 since it's the lowest common denominator shipped with
   // Ubuntu
   CEC_config->clientVersion = CEC_CLIENT_VERSION_1_6_0;
#endif
   CEC_config->bActivateSource = 0;
   CEC_config->deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);


   //  libcec callbacks
   CEC_callbacks = new ICECCallbacks();
#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 7 )
   CEC_callbacks->Clear();
#endif
   CEC_callbacks->CBCecLogMessage = log_cb;
   CEC_callbacks->CBCecKeyPress = keypress_cb;
   CEC_callbacks->CBCecCommand = command_cb;
   CEC_callbacks->CBCecConfigurationChanged = config_cb;
   CEC_callbacks->CBCecAlert = alert_cb;
   CEC_callbacks->CBCecMenuStateChanged = menu_cb;
   CEC_callbacks->CBCecSourceActivated = activated_cb;

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

   PyObject * m = Py_InitModule("cec", CecMethods);

   if( m == NULL ) return;

   Py_INCREF(dev);
   PyModule_AddObject(m, "Device", (PyObject*)dev);

   // constants for event types
   PyModule_AddIntMacro(m, EVENT_LOG);
   PyModule_AddIntMacro(m, EVENT_KEYPRESS);
   PyModule_AddIntMacro(m, EVENT_COMMAND);
   PyModule_AddIntMacro(m, EVENT_CONFIG_CHANGE);
   PyModule_AddIntMacro(m, EVENT_ALERT);
   PyModule_AddIntMacro(m, EVENT_MENU_CHANGED);
   PyModule_AddIntMacro(m, EVENT_ACTIVATED);
   PyModule_AddIntMacro(m, EVENT_ALL);

   // constants for alert types
   PyModule_AddIntConstant(m, "CEC_ALERT_SERVICE_DEVICE",
         CEC_ALERT_SERVICE_DEVICE);
   PyModule_AddIntConstant(m, "CEC_ALERT_CONNECTION_LOST",
         CEC_ALERT_CONNECTION_LOST);
   PyModule_AddIntConstant(m, "CEC_ALERT_PERMISSION_ERROR",
         CEC_ALERT_PERMISSION_ERROR);
   PyModule_AddIntConstant(m, "CEC_ALERT_PORT_BUSY",
         CEC_ALERT_PORT_BUSY);
   PyModule_AddIntConstant(m, "CEC_ALERT_PHYSICAL_ADDRESS_ERROR",
         CEC_ALERT_PHYSICAL_ADDRESS_ERROR);
   PyModule_AddIntConstant(m, "CEC_ALERT_TV_POLL_FAILED",
         CEC_ALERT_TV_POLL_FAILED);

   // constants for menu events
   PyModule_AddIntConstant(m, "CEC_MENU_STATE_ACTIVATED",
         CEC_MENU_STATE_ACTIVATED);
   PyModule_AddIntConstant(m, "CEC_MENU_STATE_DEACTIVATED",
         CEC_MENU_STATE_DEACTIVATED);

   // expose whether or not we're using the new cec_adapter_descriptor API
   // this should help debugging by exposing which version was detected and
   // which adapter detection API was used at compile time
   PyModule_AddIntMacro(m, HAVE_CEC_ADAPTER_DESCRIPTOR);
}
