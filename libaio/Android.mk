LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX -DOS_ANDROID -DOS_LINUX_KERNEL
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include

LOCAL_SRC_FILES += $(wildcard src/*.c)
LOCAL_SRC_FILES += $(wildcard src/*.cpp)
LOCAL_SRC_FILES += ../source/port/aio-socket-epoll.c
LOCAL_SRC_FILES += ../source/twtimer.c

LOCAL_MODULE := aio
include $(BUILD_SHARED_LIBRARY)
