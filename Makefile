ifdef PLATFORM
	CROSS:=$(PLATFORM)-
else
	CROSS:=
	PLATFORM:=linux
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