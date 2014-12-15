ROOT:=..
NOVERSION = 1

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 0
OUTFILE = test

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . $(ROOT)/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = . $(ROOT)/source $(ROOT)/string
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))

_SOURCE_FILES = $(ROOT)/source/HttpHelper.cpp 
_SOURCE_FILES += $(ROOT)/source/aio-socket-iocp.c
_SOURCE_FILES += $(ROOT)/source/aio-socket-kqueue.c
_SOURCE_FILES += $(ROOT)/source/serial-port-win32.c
_SOURCE_FILES += $(ROOT)/source/win32-async-pipe.c
_SOURCE_FILES += $(ROOT)/source/sysvolume.cpp
_SOURCE_FILES += $(ROOT)/string/snprintf.c
_SOURCE_FILES += $(ROOT)/string/strndup.c
_SOURCE_FILES += http-test.c rtsp-test.c sdp-test.c
SOURCE_FILES := $(filter-out $(_SOURCE_FILES),$(SOURCE_FILES))

#-----------------------------Library--------------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
LIBPATHS =
ifdef RELEASE
# relase library path
LIBPATHS +=
else
LIBPATHS +=
endif

LIBS = rt pthread dl bsd

STATIC_LIBS =

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = OS_INT64_TYPE


ifdef HTTP_TEST
SOURCE_FILES += http-test.c
SOURCE_FILES += $(ROOT)/libhttp/source/http-reason.c
SOURCE_FILES += $(ROOT)/libhttp/source/http-header-content-type.c
SOURCE_FILES += $(ROOT)/libhttp/source/http-header-host.c
SOURCE_FILES += $(ROOT)/libhttp/source/http-cookie.c
DEFINES += HTTP_TEST
INCLUDES += $(ROOT)/libhttp/include
endif

ifdef RTSP_TEST
SOURCE_FILES += rtsp-test.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-range.c 
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-rtp-info.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-session.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-transport.c
DEFINES += RTSP_TEST
INCLUDES += $(ROOT)/../media-server/librtsp/include
endif

ifdef SDP_TEST
SOURCE_FILES += sdp-test.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-fmtp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-rtpmap.c
DEFINES += SDP_TEST
INCLUDES += $(ROOT)/../media-server/librtsp/include
endif

include $(ROOT)/gcc.mk
