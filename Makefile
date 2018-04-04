all:
	$(MAKE) -C libaio
	$(MAKE) -C libhttp
	$(MAKE) -C test
	
clean:
	$(MAKE) -C libaio clean
	$(MAKE) -C libhttp clean
	$(MAKE) -C test clean
