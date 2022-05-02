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
INCLUDES = . $(ROOT)/include $(ROOT)/libaio/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = . $(ROOT)/source $(ROOT)/source/string $(ROOT)/source/digest $(ROOT)/source/port
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))
SOURCE_FILES += $(ROOT)/deprecated/tools.c
SOURCE_FILES += $(ROOT)/libaio/test/aio-poll-test.c

_SOURCE_FILES += $(ROOT)/source/port/aio-socket-iocp.c
_SOURCE_FILES += $(ROOT)/source/port/aio-socket-epoll.c
_SOURCE_FILES += $(ROOT)/source/port/aio-socket-kqueue.c
_SOURCE_FILES += $(ROOT)/source/port/serial-port-win32.c
_SOURCE_FILES += $(ROOT)/source/port/file-watcher-win32.c
_SOURCE_FILES += $(ROOT)/source/port/win32-async-pipe.c
_SOURCE_FILES += $(ROOT)/source/port/win32-minidump.c
_SOURCE_FILES += $(ROOT)/source/port/win32-backtrace.c
_SOURCE_FILES += $(ROOT)/source/port/sysvolume.cpp
_SOURCE_FILES += $(ROOT)/source/string/snprintf.c
_SOURCE_FILES += $(ROOT)/source/string/strndup.c
_SOURCE_FILES += ./http-test.c
_SOURCE_FILES += ./rtsp-test.c 
_SOURCE_FILES += ./sdp-test.c
SOURCE_FILES := $(filter-out $(_SOURCE_FILES),$(SOURCE_FILES))

#-----------------------------Library--------------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
LIBPATHS = $(ROOT)/libaio/$(BUILD).$(PLATFORM)
ifdef RELEASE
# relase library path
LIBPATHS +=
else
LIBPATHS +=
endif

LIBS = rt pthread dl aio #ssl crypto

STATIC_LIBS = $(ROOT)/libhttp/$(BUILD).$(PLATFORM)/libhttp.a

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = OS_INT64_TYPE


ifdef HTTP_TEST
SOURCE_FILES += http-test.c
SOURCE_FILES += $(ROOT)/libhttp/test/http-client-test.cpp
SOURCE_FILES += $(ROOT)/libhttp/test/http-client-test2.cpp
DEFINES += HTTP_TEST
INCLUDES += $(ROOT)/libhttp/include
endif

ifdef RTSP_TEST
SOURCE_FILES += sdp-test.c
SOURCE_FILES += rtsp-test.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-range.c 
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-rtp-info.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-session.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-transport.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-fmtp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-rtpmap.c
DEFINES += SDP_TEST
DEFINES += RTSP_TEST
INCLUDES += $(ROOT)/../media-server/librtsp/include
endif

include $(ROOT)/gcc.mk

CXXFLAGS += -std=c++0x
