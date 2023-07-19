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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libcec/cec.h>
#include <algorithm>
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
#if CEC_LIB_VERSION_MAJOR >= 3 || (CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MINOR >= 1)
#define CEC_ADAPTER_TYPE cec_adapter_descriptor
#define CEC_FIND_ADAPTERS DetectAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 1
#else
#define CEC_ADAPTER_TYPE cec_adapter
#define CEC_FIND_ADAPTERS FindAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 0
#endif

int parse_physical_addr(const char * addr) {
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
      count = (std::min)(count, cec_count);
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
         Py_BEGIN_ALLOW_THREADS
         CECDestroy(CEC_adapter);
         CEC_adapter = NULL;
         Py_END_ALLOW_THREADS
         char errstr[1024];
         snprintf(errstr, 1024, "CEC failed to open %s", dev);
         PyErr_SetString(PyExc_IOError, errstr);
      }
   }

   return result;
}

static PyObject * close(PyObject * self, PyObject * args) {

   if( CEC_adapter != NULL ) {
      Py_BEGIN_ALLOW_THREADS
      CEC_adapter->Close();
      Py_END_ALLOW_THREADS
   }

   Py_INCREF(Py_None);
   return Py_None;
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

struct Callback {
   public:
      long int event;
      PyObject * cb;

      Callback(long int e, PyObject * c) : event(e), cb(c) {
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

      Py_INCREF(callback);
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
  Py_ssize_t events = EVENT_ALL; // default to all events

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
              Py_DECREF(callback);
           }
        }
     }
     
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject * make_bound_method_args(PyObject * self, PyObject * args) {
   Py_ssize_t count = 0;
   if( PyTuple_Check(args) ) {
      count = PyTuple_Size(args);
   }
   PyObject * result = PyTuple_New(count+1);
   if( result == NULL ) {
      return NULL;
   }
   assert(self != NULL);
   Py_INCREF(self);
   PyTuple_SetItem(result, 0, self);
   for( Py_ssize_t i=0; i<count; i++ ) {
      PyObject * arg = PyTuple_GetItem(args, i);
      if( arg == NULL ) {
         Py_DECREF(result);
         return NULL;
      }
      Py_INCREF(arg);
      PyTuple_SetItem(result, i+1, arg);
   }
   return result;
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
         PyObject * callback = itr->cb;
         PyObject * arguments = args;
         if( PyMethod_Check(itr->cb) ) {
            callback = PyMethod_Function(itr->cb);
            PyObject * self = PyMethod_Self(itr->cb);
            if( self ) {
               // bound method, prepend self/cls to argument tuple
               arguments = make_bound_method_args(self, args);
            }
         }
         // see also: PyObject_CallFunction(...) which can take C args
         PyObject * temp = PyObject_CallObject(callback, arguments);
         if( arguments != args ) {
            Py_XDECREF(arguments);
         }
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

static PyObject * transmit(PyObject * self, PyObject * args) {
   unsigned char initiator = 'g';
   unsigned char destination;
   unsigned char opcode;
   const char * params = NULL;
   Py_ssize_t param_count = 0;

   if( PyArg_ParseTuple(args, "bb|s#b:transmit", &destination, &opcode,
         &params, &param_count, &initiator) ) {
      if( destination < 0 || destination > 15 ) {
         PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
         return NULL;
      }
      if( initiator != 'g' ) {
         if( initiator < 0 || initiator > 15 ) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
         }
      } else {
         initiator = CEC_adapter->GetLogicalAddresses().primary;
      }
      if( param_count > CEC_MAX_DATA_PACKET_SIZE ) {
         char errstr[1024];
         snprintf(errstr, 1024, "Too many parameters, maximum is %d",
            CEC_MAX_DATA_PACKET_SIZE);
         PyErr_SetString(PyExc_ValueError, errstr);
         return NULL;
      }
      cec_command data;
      bool success;
      Py_BEGIN_ALLOW_THREADS
      data.initiator = (cec_logical_address)initiator;
      data.destination = (cec_logical_address)destination;
      data.opcode = (cec_opcode)opcode;
      data.opcode_set = 1;
      if( params ) {
         for( Py_ssize_t i=0; i<param_count; i++ ) {
            data.PushBack(((uint8_t *)params)[i]);
         }
      }
      success = CEC_adapter->Transmit(data);
      Py_END_ALLOW_THREADS
      RETURN_BOOL(success);
   }

   return NULL;
}

static PyObject * is_active_source(PyObject * self, PyObject * args) {
   unsigned char addr;

   if( PyArg_ParseTuple(args, "b:is_active_source", &addr) ) {
      if( addr < 0 || addr > 15 ) {
         PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
         return NULL;
      } else {
         RETURN_BOOL(CEC_adapter->IsActiveSource((cec_logical_address)addr));
      }
   }
   return NULL;
}

static PyObject * set_active_source(PyObject * self, PyObject * args) {
   unsigned char devtype = (unsigned char)CEC_DEVICE_TYPE_RESERVED;

   if( PyArg_ParseTuple(args, "|b:set_active_source", &devtype) ) {
      if( devtype < 0 || devtype > 5 ) {
         PyErr_SetString(PyExc_ValueError, "Device type must be between 0 and 5");
         return NULL;
      } else {
         RETURN_BOOL(CEC_adapter->SetActiveSource((cec_device_type)devtype));
      }
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
#if PY_MAJOR_VERSION >= 3
      if(PyLong_Check(arg)) {
         long arg_l = PyLong_AsLong(arg);
#else
      if(PyInt_Check(arg)) {
         long arg_l = PyInt_AsLong(arg);
#endif
         Py_DECREF(arg);
         if( arg_l < 0 || arg_l > 15 ) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
         } else {
            RETURN_BOOL(CEC_adapter->SetStreamPath((cec_logical_address)arg_l));
         }
#if PY_MAJOR_VERSION >= 3
      } else if(PyUnicode_Check(arg)) {
         const char * arg_s = PyUnicode_AsUTF8(arg);
#else
      } else if(PyString_Check(arg)) {
         char * arg_s = PyString_AsString(arg);
#endif
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
#if CEC_LIB_VERSION_MAJOR >= 5
      RETURN_BOOL(CEC_adapter->CanSaveConfiguration());
#else
      RETURN_BOOL(CEC_adapter->CanPersistConfiguration());
#endif
   }
   return NULL;
}

PyObject * persist_config(PyObject * self, PyObject * args) {
   if( PyArg_ParseTuple(args, ":persist_config") ) {
#if CEC_LIB_VERSION_MAJOR >= 5
      if( ! CEC_adapter->CanSaveConfiguration() ) {
#else
      if( ! CEC_adapter->CanPersistConfiguration() ) {
#endif
         PyErr_SetString(PyExc_NotImplementedError,
               "Cannot persist configuration");
         return NULL;
      }
      libcec_configuration config;
      if( ! CEC_adapter->GetCurrentConfiguration(&config) ) {
         PyErr_SetString(PyExc_IOError, "Could not get configuration");
         return NULL;
      }
#if CEC_LIB_VERSION_MAJOR >= 5
      RETURN_BOOL(CEC_adapter->SetConfiguration(&config));
#else
      RETURN_BOOL(CEC_adapter->PersistConfiguration(&config));
#endif
   }
   return NULL;
}

static PyMethodDef CecMethods[] = {
   {"list_adapters", list_adapters, METH_VARARGS, "List available adapters"},
   {"init", init, METH_VARARGS, "Open an adapter"},
   {"close", close, METH_NOARGS, "Close an adapter"},
   {"list_devices", list_devices, METH_VARARGS, "List devices"},
   {"add_callback", add_callback, METH_VARARGS, "Add a callback"},
   {"remove_callback", remove_callback, METH_VARARGS, "Remove a callback"},
   {"transmit", transmit, METH_VARARGS, "Transmit a raw CEC command"},
   {"is_active_source", is_active_source, METH_VARARGS, "Check active source"},
   {"set_active_source", set_active_source, METH_VARARGS, "Set active source"},
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

#if CEC_LIB_VERSION_MAJOR >= 4
void log_cb(void * self, const cec_log_message* message) {
#else
int log_cb(void * self, const cec_log_message message) {
#endif
   debug("got log callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
   int level = message->level;
   long int time = message->time;
   const char* msg = message->message;
#else
   int level = message.level;
   long int time = message.time;
   const char* msg = message.message;
#endif
   // decode message ignoring invalid characters
   PyObject * umsg = PyUnicode_DecodeASCII(msg, strlen(msg), "ignore");
   PyObject * args = Py_BuildValue("(iilO)", EVENT_LOG,
         level,
         time,
         umsg);
   if( args ) {
      trigger_event(EVENT_LOG, args);
      Py_DECREF(args);
   }
   Py_XDECREF(umsg);
   PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
   return;
#else
   return 1;
#endif
}

#if CEC_LIB_VERSION_MAJOR >= 4
void keypress_cb(void * self, const cec_keypress* key) {
#else
int keypress_cb(void * self, const cec_keypress key) {
#endif
   debug("got keypress callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
   cec_user_control_code keycode = key->keycode;
   unsigned int duration = key->duration;
#else
   cec_user_control_code keycode = key.keycode;
   unsigned int duration = key.duration;
#endif
   PyObject * args = Py_BuildValue("(iBI)", EVENT_KEYPRESS,
         keycode,
         duration);
   if( args ) {
      trigger_event(EVENT_KEYPRESS, args);
      Py_DECREF(args);
   }
   PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
   return;
#else
   return 1;
#endif
}

static PyObject * convert_cmd(const cec_command* cmd) {
#if PY_MAJOR_VERSION >= 3
   return Py_BuildValue("{sBsBsOsOsBsy#sOsi}",
#else
   return Py_BuildValue("{sBsBsOsOsBss#sOsi}",
#endif
         "initiator", cmd->initiator,
         "destination", cmd->destination,
         "ack", cmd->ack ? Py_True : Py_False,
         "eom", cmd->eom ? Py_True : Py_False,
         "opcode", cmd->opcode,
         "parameters", cmd->parameters.data, cmd->parameters.size,
         "opcode_set", cmd->opcode_set ? Py_True : Py_False,
         "transmit_timeout", cmd->transmit_timeout);
}

#if CEC_LIB_VERSION_MAJOR >= 4
void command_cb(void * self, const cec_command* command) {
#else
int command_cb(void * self, const cec_command command) {
#endif
   debug("got command callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
   const cec_command * cmd = command;
#else
   const cec_command * cmd = &command;
#endif
   PyObject * args = Py_BuildValue("(iO&)", EVENT_COMMAND, convert_cmd, cmd);
   if( args ) {
      trigger_event(EVENT_COMMAND, args);
      Py_DECREF(args);
   }
   PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
   return;
#else
   return 1;
#endif
}

#if CEC_LIB_VERSION_MAJOR >= 4
void config_cb(void * self, const libcec_configuration*) {
#else
int config_cb(void * self, const libcec_configuration) {
#endif
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
   if( args ) {
      // don't bother triggering an event until we can actually pass arguments
      //trigger_event(EVENT_CONFIG_CHANGE, args);
      Py_DECREF(args);
   }
   PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
   return;
#else
   return 1;
#endif
}

#if CEC_LIB_VERSION_MAJOR >= 4
void alert_cb(void * self, const libcec_alert alert, const libcec_parameter p) {
#else
int alert_cb(void * self, const libcec_alert alert, const libcec_parameter p) {
#endif
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
   if( args ) {
      trigger_event(EVENT_ALERT, args);
      Py_DECREF(args);
   }
   PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
   return;
#else
   return 1;
#endif
}

int menu_cb(void * self, const cec_menu_state menu) {
   debug("got menu callback\n");
   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();
   PyObject * args = Py_BuildValue("(ii)", EVENT_MENU_CHANGED, menu);
   if( args ) {
      trigger_event(EVENT_MENU_CHANGED, args);
      Py_DECREF(args);
   }
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
   if( args ) {
      trigger_event(EVENT_ACTIVATED, args);
      Py_DECREF(args);
   }
   PyGILState_Release(gstate);
   return;
}

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
   PyModuleDef_HEAD_INIT,
   "cec",
   NULL,
   -1,
   CecMethods,
   NULL,
   NULL,
   NULL,
   NULL
};
#endif

#if PY_MAJOR_VERSION >= 3
#define INITERROR return NULL
#else
#define INITERROR return
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit_cec(void) {
#else
PyMODINIT_FUNC initcec(void) {
#endif
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
#if CEC_LIB_VERSION_MAJOR >= 4
   CEC_callbacks->logMessage = log_cb;
   CEC_callbacks->keyPress = keypress_cb;
   CEC_callbacks->commandReceived = command_cb;
   CEC_callbacks->configurationChanged = config_cb;
   CEC_callbacks->alert = alert_cb;
   CEC_callbacks->menuStateChanged = menu_cb;
   CEC_callbacks->sourceActivated = activated_cb;
#else
   CEC_callbacks->CBCecLogMessage = log_cb;
   CEC_callbacks->CBCecKeyPress = keypress_cb;
   CEC_callbacks->CBCecCommand = command_cb;
   CEC_callbacks->CBCecConfigurationChanged = config_cb;
   CEC_callbacks->CBCecAlert = alert_cb;
   CEC_callbacks->CBCecMenuStateChanged = menu_cb;
   CEC_callbacks->CBCecSourceActivated = activated_cb;
#endif

   CEC_config->callbacks = CEC_callbacks;

   CEC_adapter = (ICECAdapter*)CECInitialise(CEC_config);

   if( !CEC_adapter ) {
      PyErr_SetString(PyExc_IOError, "Failed to initialize libcec");
      INITERROR;
   }

#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 8 )
   CEC_adapter->InitVideoStandalone();
#endif

   // set up python module
   PyTypeObject * dev = DeviceTypeInit(CEC_adapter);
   Device = (PyObject*)dev;
   if(PyType_Ready(dev) < 0 ) INITERROR;

#if PY_MAJOR_VERSION >= 3
   PyObject * m = PyModule_Create(&moduledef);
#else
   PyObject * m = Py_InitModule("cec", CecMethods);
#endif

   if( m == NULL ) INITERROR;

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

   // constants for device types
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_TV",
         CEC_DEVICE_TYPE_TV);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_RECORDING_DEVICE",
         CEC_DEVICE_TYPE_RECORDING_DEVICE);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_RESERVED",
         CEC_DEVICE_TYPE_RESERVED);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_TUNER",
         CEC_DEVICE_TYPE_TUNER);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_PLAYBACK_DEVICE",
         CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_AUDIO_SYSTEM",
         CEC_DEVICE_TYPE_AUDIO_SYSTEM);

   // constants for logical addresses
   PyModule_AddIntConstant(m, "CECDEVICE_UNKNOWN",
         CECDEVICE_UNKNOWN);
   PyModule_AddIntConstant(m, "CECDEVICE_TV",
         CECDEVICE_TV);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE1",
         CECDEVICE_RECORDINGDEVICE1);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE2",
         CECDEVICE_RECORDINGDEVICE2);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER1",
         CECDEVICE_TUNER1);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE1",
         CECDEVICE_PLAYBACKDEVICE1);
   PyModule_AddIntConstant(m, "CECDEVICE_AUDIOSYSTEM",
         CECDEVICE_AUDIOSYSTEM);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER2",
         CECDEVICE_TUNER2);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER3",
         CECDEVICE_TUNER3);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE2",
         CECDEVICE_PLAYBACKDEVICE2);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE3",
         CECDEVICE_RECORDINGDEVICE3);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER4",
         CECDEVICE_TUNER4);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE3",
         CECDEVICE_PLAYBACKDEVICE3);
   PyModule_AddIntConstant(m, "CECDEVICE_RESERVED1",
         CECDEVICE_RESERVED1);
   PyModule_AddIntConstant(m, "CECDEVICE_RESERVED2",
         CECDEVICE_RESERVED2);
   PyModule_AddIntConstant(m, "CECDEVICE_FREEUSE",
         CECDEVICE_FREEUSE);
   PyModule_AddIntConstant(m, "CECDEVICE_UNREGISTERED",
         CECDEVICE_UNREGISTERED);
   PyModule_AddIntConstant(m, "CECDEVICE_BROADCAST",
         CECDEVICE_BROADCAST);

   // constants for opcodes
   PyModule_AddIntConstant(m, "CEC_OPCODE_ACTIVE_SOURCE",
         CEC_OPCODE_ACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_IMAGE_VIEW_ON",
         CEC_OPCODE_IMAGE_VIEW_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TEXT_VIEW_ON",
         CEC_OPCODE_TEXT_VIEW_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_INACTIVE_SOURCE",
         CEC_OPCODE_INACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ACTIVE_SOURCE",
         CEC_OPCODE_REQUEST_ACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ROUTING_CHANGE",
         CEC_OPCODE_ROUTING_CHANGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ROUTING_INFORMATION",
         CEC_OPCODE_ROUTING_INFORMATION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_STREAM_PATH",
         CEC_OPCODE_SET_STREAM_PATH);
   PyModule_AddIntConstant(m, "CEC_OPCODE_STANDBY",
         CEC_OPCODE_STANDBY);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_OFF",
         CEC_OPCODE_RECORD_OFF);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_ON",
         CEC_OPCODE_RECORD_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_STATUS",
         CEC_OPCODE_RECORD_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_TV_SCREEN",
         CEC_OPCODE_RECORD_TV_SCREEN);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_ANALOGUE_TIMER",
         CEC_OPCODE_CLEAR_ANALOGUE_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_DIGITAL_TIMER",
         CEC_OPCODE_CLEAR_DIGITAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_EXTERNAL_TIMER",
         CEC_OPCODE_CLEAR_EXTERNAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_ANALOGUE_TIMER",
         CEC_OPCODE_SET_ANALOGUE_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_DIGITAL_TIMER",
         CEC_OPCODE_SET_DIGITAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_EXTERNAL_TIMER",
         CEC_OPCODE_SET_EXTERNAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_TIMER_PROGRAM_TITLE",
         CEC_OPCODE_SET_TIMER_PROGRAM_TITLE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TIMER_CLEARED_STATUS",
         CEC_OPCODE_TIMER_CLEARED_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TIMER_STATUS",
         CEC_OPCODE_TIMER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CEC_VERSION",
         CEC_OPCODE_CEC_VERSION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GET_CEC_VERSION",
         CEC_OPCODE_GET_CEC_VERSION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_PHYSICAL_ADDRESS",
         CEC_OPCODE_GIVE_PHYSICAL_ADDRESS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GET_MENU_LANGUAGE",
         CEC_OPCODE_GET_MENU_LANGUAGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_PHYSICAL_ADDRESS",
         CEC_OPCODE_REPORT_PHYSICAL_ADDRESS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_MENU_LANGUAGE",
         CEC_OPCODE_SET_MENU_LANGUAGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DECK_CONTROL",
         CEC_OPCODE_DECK_CONTROL);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DECK_STATUS",
         CEC_OPCODE_DECK_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DECK_STATUS",
         CEC_OPCODE_GIVE_DECK_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_PLAY",
         CEC_OPCODE_PLAY);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS",
         CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SELECT_ANALOGUE_SERVICE",
         CEC_OPCODE_SELECT_ANALOGUE_SERVICE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SELECT_DIGITAL_SERVICE",
         CEC_OPCODE_SELECT_DIGITAL_SERVICE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_DEVICE_STATUS",
         CEC_OPCODE_TUNER_DEVICE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_STEP_DECREMENT",
         CEC_OPCODE_TUNER_STEP_DECREMENT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_STEP_INCREMENT",
         CEC_OPCODE_TUNER_STEP_INCREMENT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DEVICE_VENDOR_ID",
         CEC_OPCODE_DEVICE_VENDOR_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DEVICE_VENDOR_ID",
         CEC_OPCODE_GIVE_DEVICE_VENDOR_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_COMMAND",
         CEC_OPCODE_VENDOR_COMMAND);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_COMMAND_WITH_ID",
         CEC_OPCODE_VENDOR_COMMAND_WITH_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN",
         CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP",
         CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_OSD_STRING",
         CEC_OPCODE_SET_OSD_STRING);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_OSD_NAME",
         CEC_OPCODE_GIVE_OSD_NAME);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_OSD_NAME",
         CEC_OPCODE_SET_OSD_NAME);
   PyModule_AddIntConstant(m, "CEC_OPCODE_MENU_REQUEST",
         CEC_OPCODE_MENU_REQUEST);
   PyModule_AddIntConstant(m, "CEC_OPCODE_MENU_STATUS",
         CEC_OPCODE_MENU_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_USER_CONTROL_PRESSED",
         CEC_OPCODE_USER_CONTROL_PRESSED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_USER_CONTROL_RELEASE",
         CEC_OPCODE_USER_CONTROL_RELEASE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DEVICE_POWER_STATUS",
         CEC_OPCODE_GIVE_DEVICE_POWER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_POWER_STATUS",
         CEC_OPCODE_REPORT_POWER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_FEATURE_ABORT",
         CEC_OPCODE_FEATURE_ABORT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ABORT",
         CEC_OPCODE_ABORT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_AUDIO_STATUS",
         CEC_OPCODE_GIVE_AUDIO_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS",
         CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_AUDIO_STATUS",
         CEC_OPCODE_REPORT_AUDIO_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_SYSTEM_AUDIO_MODE",
         CEC_OPCODE_SET_SYSTEM_AUDIO_MODE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST",
         CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS",
         CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_AUDIO_RATE",
         CEC_OPCODE_SET_AUDIO_RATE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_START_ARC",
         CEC_OPCODE_START_ARC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_ARC_STARTED",
         CEC_OPCODE_REPORT_ARC_STARTED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_ARC_ENDED",
         CEC_OPCODE_REPORT_ARC_ENDED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ARC_START",
         CEC_OPCODE_REQUEST_ARC_START);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ARC_END",
         CEC_OPCODE_REQUEST_ARC_END);
   PyModule_AddIntConstant(m, "CEC_OPCODE_END_ARC",
         CEC_OPCODE_END_ARC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CDC",
         CEC_OPCODE_CDC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_NONE",
         CEC_OPCODE_NONE);

   // expose whether or not we're using the new cec_adapter_descriptor API
   // this should help debugging by exposing which version was detected and
   // which adapter detection API was used at compile time
   PyModule_AddIntMacro(m, HAVE_CEC_ADAPTER_DESCRIPTOR);

#if PY_MAJOR_VERSION >= 3
   return m;
#endif
}
