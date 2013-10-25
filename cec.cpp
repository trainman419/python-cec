/* cec.cpp
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

std::list<cec_adapter_descriptor> get_adapters() {
   std::list<cec_adapter_descriptor> res;
   // release the Global Interpreter lock
   Py_BEGIN_ALLOW_THREADS
   // get adapters
   int cec_count = 10;
   cec_adapter_descriptor * dev_list = (cec_adapter_descriptor*)malloc(
         cec_count * sizeof(cec_adapter_descriptor));
   int count = CEC_adapter->DetectAdapters(dev_list, cec_count);
   if( count > cec_count ) {
      cec_count = count;
      dev_list = (cec_adapter_descriptor*)realloc(dev_list, 
         cec_count * sizeof(cec_adapter_descriptor));
      count = CEC_adapter->DetectAdapters(dev_list, cec_count);
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
      std::list<cec_adapter_descriptor> dev_list = get_adapters();
      // set up our result list
      result = PyList_New(0);

      // populate our result list
      std::list<cec_adapter_descriptor>::const_iterator itr;
      for( itr = dev_list.begin(); itr != dev_list.end(); itr++ ) {
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
      }
   }

   return result;
}

static PyObject * init(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   const char * dev = NULL;

   // this is NOT the right way to parse optional args in C...
   //  it doesn't produce good error messages; sometimes reports 0 args, other
   //  time 1 required
   // HOWEVER, it works.     Bitches.
   if( PyArg_ParseTuple(args, ":init") ) {
      std::list<cec_adapter_descriptor> devs = get_adapters();
      if( devs.size() > 0 ) {
         dev = devs.front().strComName;
      } else {
         PyErr_SetString(PyExc_Exception, "No default adapter found");
      }
   } else {
      PyArg_ParseTuple(args, "s:init", &dev);
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

static PyObject * add_callback(PyObject * self, PyObject * args) {
   PyObject * result = NULL;
   PyObject * temp;

   if( PyArg_ParseTuple(args, "O:add_callback", &temp) ) {
      if( !PyCallable_Check(temp)) {
         PyErr_SetString(PyExc_TypeError, "parameter must be callable");
         return NULL;
      }
      open_callbacks->add(temp);
      Py_INCREF(Py_None);
      result = Py_None;
   }
   return result;
}

static PyObject * volume_up(PyObject * self, PyObject * args) {
   PyObject * result = NULL;

   if( PyArg_ParseTuple(args, ":volume_up") ) {
      CEC_adapter->VolumeUp();
      Py_INCREF(Py_None);
      result = Py_None;
   }

   return result;
}

static PyObject * volume_down(PyObject * self, PyObject * args) {
   PyObject * result = NULL;

   if( PyArg_ParseTuple(args, ":volume_up") ) {
      CEC_adapter->VolumeUp();
      Py_INCREF(Py_None);
      result = Py_None;
   }

   return result;
}

static PyMethodDef CecMethods[] = {
   {"list_adapters", list_adapters, METH_VARARGS, "List available adapters"},
   {"init", init, METH_VARARGS, "Open an adapter"},
   {"add_callback", add_callback, METH_VARARGS, "Add a callback"},
   {"volume_up",   volume_up,   METH_VARARGS, "Volume Up"},
   {"volume_down", volume_down, METH_VARARGS, "Volume Down"},
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
   CEC_config->clientVersion = CEC_CLIENT_VERSION_CURRENT;
   CEC_config->bActivateSource = 0;
   CEC_config->deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);


   //  libcec callbacks
   CEC_callbacks = new ICECCallbacks();
   CEC_callbacks->Clear();

   CEC_config->callbacks = CEC_callbacks;

   CEC_adapter = (ICECAdapter*)CECInitialise(CEC_config);

   if( !CEC_adapter ) {
      PyErr_SetString(PyExc_IOError, "Failed to initialize libcec");
      return;
   }

   CEC_adapter->InitVideoStandalone();

   // set up python module
   PyTypeObject * dev = DeviceTypeInit(CEC_adapter);
   if(PyType_Ready(dev) < 0 ) return;

   PyObject * m = Py_InitModule("cec", CecMethods);

   if( m == NULL ) return;

   Py_INCREF(dev);
   PyModule_AddObject(m, "Device", (PyObject*)dev);
}
