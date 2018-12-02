#ifndef _ice_internal_h_
#define _ice_internal_h_

#define STUN_PORT 3478 // both TCP and UDP, rfc3489 8.1 Binding Requests (p10)

//rfc3489 8.2 Shared Secret Requests(p13)
#define STUN_SHARED_SECRET_PERIOD (10*60*1000) // 10m
#define STUN_SHARED_SECRET_EXPIRE (30*60*1000) // 30m

// 0ms, 100ms, 300ms, 700ms, 1500ms, 3100ms, 4700ms, 6300ms, and 7900ms ==> At 9500ms,
#define STUN_TIMEOUT 9500
#define STUN_RETRANSMISSION_INTERVAL_MIN 100 //ms
#define STUN_RETRANSMISSION_INTERVAL_MAX 1600 // ms

enum ice_checklist_state_t
{
	ICE_CHECKLIST_RUNNING = 1,
	ICE_CHECKLIST_COMPLETED,
	ICE_CHECKLIST_FAILED,
};

// rfc3489 7. Message Overview(p8)
#define STUN_MSG_BIND_REQUEST					0x0001
#define STUN_MSG_BIND_RESPONSE					0x0101
#define STUN_MSG_BIND_ERROR_RESPONSE			0x0111
#define STUN_MSG_SHARED_SECRET_REQUEST			0x0002
#define STUN_MSG_SHARED_SECRET_RESPONSE			0x0102
#define STUN_MSG_SHARED_SECRET_ERROR_RESPONSE	0x0112

enum stun_message_attribute_t
{
	STUN_ATTR_MAPPED_ADDRESS = 1, // bind response only(ip + port)
	STUN_ATTR_RESPONSE_ADDRESS, // optional
	STUN_ATTR_CHANGE_REQUEST, // optional, bind request only
	STUN_ATTR_SOURCE_ADDRESS,
	STUN_ATTR_CHANGED_ADDRESS, // bind response 
	STUN_ATTR_USERNAME, // shared secret response/bind request
	STUN_ATTR_PASSWORD, // shared secret response only
	STUN_ATTR_MESSAGE_INTEGRITY, // bind request/response, MUST be the last attribute within a message.
	STUN_ATTR_ERROR_CODE, // bind error response/shared secret error response
	STUN_ATTR_UNKNOWN_ATTRIBUTES, // bind error response/shared secret error response
	STUN_ATTR_REFLECTED_FROM, // bind response

	STUN_ATTR_END = 0x7fff,
};

// rfc 3489 11.1 Message Header (p25)
struct stun_header_t
{
	uint16_t msgtype;
	uint16_t length; // payload length, don't include header
	uint8_t transaction_id[16];
}

#define STUN_ERROR_CODE_BAD_REQUEST 400
#define STUN_ERROR_CODE_UNAUTHORIZED 401
#define STUN_ERROR_CODE_UNKNOWN_ATTRIBUTE 420
#define STUN_ERROR_CODE_STALE_CREDENTIALS 430
#define STUN_ERROR_CODE_INTEGRITY_CHECK_FAILURE 431
#define STUN_ERROR_CODE_MISSING_USERNAME 432
#define STUN_ERROR_CODE_USE_TLS 433
#define STUN_ERROR_CODE_SERVER_ERROR 500
#define STUN_ERROR_CODE_GLOBAL_FAILURE 600

// rounded-time is the current time modulo 20 minutes
// USERNAME = <prefix,rounded-time,clientIP,hmac>
// password = <hmac(USERNAME,anotherprivatekey)>

#endif /* !_ice_internal_h_ */
