#ifndef _stun_proto_h_
#define _stun_proto_h_

#define STUN_PORT        3478 // both TCP and UDP, rfc3489 8.1 Binding Requests (p10)
#define STUN_TLS_PORT    5349 // The default port for STUN over TLS is 5349 (p22)

#define STUN_HEADER_SIZE 20
#define STUN_SOFTWARE    "libic"

// rfc5389 15.5. FINGERPRINT
#define STUN_FINGERPRINT_XOR 0x5354554e

// rfc5389 6. STUN Message Structure (p11)
#define STUN_MAGIC_COOKIE    0x2112A442

// rfc5766 14.7. REQUESTED-TRANSPORT (p47)
#define TURN_TRANSPORT_UDP   17 // UDP
#define TURN_LIFETIME        600 // seconds

//rfc3489 8.2 Shared Secret Requests(p13)
#define STUN_SHARED_SECRET_PERIOD (10*60*1000) // 10m
#define STUN_SHARED_SECRET_EXPIRE (30*60*1000) // 30m

// 0ms, 100ms, 300ms, 700ms, 1500ms, 3100ms, 4700ms, 6300ms, and 7900ms ==> At 9500ms,
#define STUN_TIMEOUT 9500
#define STUN_RETRANSMISSION_INTERVAL_MIN 100 //ms
#define STUN_RETRANSMISSION_INTERVAL_MAX 1600 // ms

// rfc5389 6. STUN Message Structure(p10)
#define STUN_METHOD_CLASS_REQUEST           0x0000 //0b00
#define STUN_METHOD_CLASS_INDICATION        0x0010 //0b01
#define STUN_METHOD_CLASS_SUCCESS_RESPONSE  0x0100 //0b10
#define STUN_METHOD_CLASS_FAILURE_RESPONSE  0x0110 //0b11

// STUN Method
enum
{
    STUN_METHOD_BIND                = 0x001,
    STUN_METHOD_SHARED_SECRET       = 0x002,
    STUN_METHOD_ALLOCATE            = 0x003, // rfc5766 only request/response semantics defined
    STUN_METHOD_REFRESH             = 0x004, // rfc5766 only request/response semantics defined
    STUN_METHOD_SEND                = 0x006, // rfc5766 only indication semantics defined
    STUN_METHOD_DATA                = 0x007, // rfc5766 only indication semantics defined
    STUN_METHOD_CREATE_PERMISSION   = 0x008, // rfc5766 only request/response semantics defined
    STUN_METHOD_CHANNEL_BIND        = 0x009, // rfc5766 only request/response semantics defined
};

// STUN Attribute
enum
{
    // Comprehension-required range (0x0000-0x7FFF):
    STUN_ATTR_MAPPED_ADDRESS         = 0x0001, // bind response only(ip + port)
    STUN_ATTR_RESPONSE_ADDRESS       = 0x0002, // optional(deprecated)
    STUN_ATTR_CHANGE_REQUEST         = 0x0003, // optional, bind request only(deprecated)
    STUN_ATTR_SOURCE_ADDRESS         = 0x0004, // (deprecated)
    STUN_ATTR_CHANGED_ADDRESS        = 0x0005, // bind response(deprecated)
    STUN_ATTR_USERNAME               = 0x0006, // shared secret response/bind request
    STUN_ATTR_PASSWORD               = 0x0007, // shared secret response only(deprecated)
    STUN_ATTR_MESSAGE_INTEGRITY      = 0x0008, // bind request/response, MUST be the last attribute within a message.
    STUN_ATTR_ERROR_CODE             = 0x0009, // bind error response/shared secret error response
    STUN_ATTR_UNKNOWN_ATTRIBUTES     = 0x000A, // bind error response/shared secret error response
    STUN_ATTR_REFLECTED_FROM         = 0x000B, // bind response(deprecated)
    STUN_ATTR_CHANNEL_NUMBER         = 0x000C, // rfc5766
    STUN_ATTR_LIFETIME               = 0x000D, // rfc5766
    STUN_ATTR_BANDWIDTH              = 0x0010, // rfc5766 (deprecated)
    STUN_ATTR_XOR_PEER_ADDRESS       = 0x0012, // rfc5766
    STUN_ATTR_DATA                   = 0x0013, // rfc5766
    STUN_ATTR_REALM                  = 0x0014, // rfc5389 realm
    STUN_ATTR_NONCE                  = 0x0015, // rfc5389 nonce
    STUN_ATTR_XOR_RELAYED_ADDRESS    = 0x0016, // rfc5766
    STUN_ATTR_EVEN_PORT              = 0x0018, // rfc5766
    STUN_ATTR_REQUESTED_TRANSPORT    = 0x0019, // rfc5766
    STUN_ATTR_DONT_FRAGMENT          = 0x001A, // rfc5766
    STUN_ATTR_XOR_MAPPED_ADDRESS     = 0x0020, // rfc5389 xor-mapped-address
    STUN_ATTR_TIMER_VAL              = 0x0021, // rfc5766
    STUN_ATTR_RESERVATION_TOKEN      = 0x0022, // rfc5766
    STUN_ATTR_PRIORITY               = 0x0024, // rfc5245
    STUN_ATTR_USE_CANDIDATE          = 0x0025, // rfc5245
    
    STUN_ATTR_COMPREHENSION_REQUIRED = 0x7fff,
    
    // Comprehension-optional range (0x8000-0xFFFF)
    STUN_ATTR_SOFTWARE               = 0x8022, // rfc5389
    STUN_ATTR_ALTERNATE_SERVER       = 0x8023, // rfc5389
    STUN_ATTR_FIGNERPRINT            = 0x8028, // rfc5389
    STUN_ATTR_ICE_CONTROLLED         = 0x8029, // rfc5245
    STUN_ATTR_ICE_CONTROLLING        = 0x802a, // rfc5245
};

#define STUN_ERROR_CODE_BAD_REQUEST                     400
#define STUN_ERROR_CODE_UNAUTHORIZED                    401
#define STUN_ERROR_CODE_FORBIDDEN                       403 // rfc5766
#define STUN_ERROR_CODE_UNKNOWN_ATTRIBUTE               420
#define STUN_ERROR_CODE_STALE_CREDENTIALS               430
#define STUN_ERROR_CODE_INTEGRITY_CHECK_FAILURE         431
#define STUN_ERROR_CODE_MISSING_USERNAME                432
#define STUN_ERROR_CODE_USE_TLS                         433
#define STUN_ERROR_CODE_ALLOCATION_MISMATCH             437 // rfc5766
#define STUN_ERROR_CODE_WRONG_CREDENTIALS               441 // rfc5766
#define STUN_ERROR_CODE_UNSUPPORTED_TRANSPORT_PROTOCO   442 // rfc5766
#define STUN_ERROR_CODE_ALLOCATION_QUOTA_REACHED        483 // rfc5766
#define STUN_ERROR_CODE_SERVER_ERROR                    500
#define STUN_ERROR_CODE_INSUFFICIENT_CAPACITY           508 // rfc5766
#define STUN_ERROR_CODE_GLOBAL_FAILURE                  600

#define ALGIN_4BYTES(v) (((v)+3) / 4 * 4)

#endif /* _stun_proto_h_ */
