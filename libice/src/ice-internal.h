#ifndef _ice_internal_h_
#define _ice_internal_h_

enum ice_checklist_state_t
{
	ICE_CHECKLIST_RUNNING = 1,
	ICE_CHECKLIST_COMPLETED,
	ICE_CHECKLIST_FAILED,
};

// rounded-time is the current time modulo 20 minutes
// USERNAME = <prefix,rounded-time,clientIP,hmac>
// password = <hmac(USERNAME,anotherprivatekey)>

#endif /* !_ice_internal_h_ */
