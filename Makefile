ARCHBITS ?=  # 32/64 bits

ifeq ($(shell uname -m), x86_64)
	ARCHBITS = 64
else ifeq ($(shell getconf LONG_BIT), 64)
	ARCHBITS = 64
else ifeq ($(shell arch), x86_64)
	ARCHBITS = 64
endif

ifdef PLATFORM
	CROSS:=$(PLATFORM)-
else
	OSID := $(shell awk -F'=' '/^ID=/ {print $$2}' /etc/os-release | tr -d '"')
	OSVERSIONID := $(shell awk -F'=' '/^VERSION_ID=/ {print $$2"-"}' /etc/os-release | tr -d '"')
	CROSS:=
	PLATFORM:=${OSID}$(OSVERSIONID)linux$(ARCHBITS)
endif

ifeq ($(RELEASE),1)
	BUILD:=release
else
	BUILD:=debug
endif

all:
	$(MAKE) -C libsdk
	$(MAKE) -C libaio
	$(MAKE) -C libhttp
	$(MAKE) -C libice
	
clean:
	$(MAKE) -C libsdk clean
	$(MAKE) -C libaio clean
	$(MAKE) -C libhttp clean
	$(MAKE) -C libice clean
	$(MAKE) -C test clean

debug:
	$(MAKE) -C libsdk   debug
	$(MAKE) -C libaio   debug
	$(MAKE) -C libhttp  debug
	$(MAKE) -C libice   debug
	$(MAKE) -C test    	debug

.PHONY : test
test:
	$(MAKE) -C test
	cd libaio/$(BUILD).$(PLATFORM) && ../../test/$(BUILD).$(PLATFORM)/test
