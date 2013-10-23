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

   uint64_t                   vendorId;
   uint16_t                   physicalAddress;
   bool                       active;
   CEC::cec_version           cecVersion;
   CEC::cec_power_status      power;
   CEC::cec_osd_name          osdName;
   char                       strAddr[8];
   CEC::cec_menu_language     lang;
};

PyTypeObject * DeviceTypeInit(CEC::ICECAdapter * adapter);
