#ifndef _ice_checklist_h_
#define _ice_checklist_h_

#include "ice-internal.h"

struct ice_checklist_t;

struct ice_checklist_t* ice_checklist_create();
int ice_checklist_destroy(struct ice_checklist_t** l);

#endif /* !_ice_checklist_h_ */
