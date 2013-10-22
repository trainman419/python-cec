/* device.cpp
 *
 * Implementation of CEC Device class for Python
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

#include "device.h"

using namespace CEC;

static PyTypeObject cec_DeviceType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /*ob_size*/
   "cec.Device",              /*tp_name*/
   sizeof(cec_DeviceObject),  /*tp_basicsize*/
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
