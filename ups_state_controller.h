// ups_state_controller.h

#ifndef UPS_STATE_CONTROLLER_H // inclusion guard
#define UPS_STATE_CONTROLLER_H

#include "types.h"
#include "ups.h"
#include "timer.h"
#include "ups_com.h"

// function prototypes

void init_ups_state_controller(struct upsDataStrucT *upsData);
void ups_state_controller(struct upsDataStrucT *upsData);
void setSnmpReboot(int duration);

#endif // _ups_state_controller_h
