RELEASE ?= 0 # default debug
UNICODE ?= 0 # default ansi

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

KERNEL := $(shell uname -s)
ifeq ($(KERNEL),Linux)
	DEFINES += OS_LINUX
endif
ifeq ($(KERNEL),Darwin)
	DEFINES += OS_MAC
endif

#--------------------------------Compile-----------------------------
#
#--------------------------------------------------------------------
AR := $(CROSS)ar
CC := $(CROSS)gcc
CXX := $(CROSS)g++
CFLAGS += -Wall -fPIC
CXXFLAGS += -Wall

ifeq ($(RELEASE),1)
	CFLAGS += -Wall -O2
	CXXFLAGS += $(CFLAGS)
	DEFINES += NDEBUG
else
	CFLAGS += -g -Wall
#	CFLAGS += -fsanitize=address
	CXXFLAGS += $(CFLAGS)
	DEFINES += DEBUG _DEBUG
endif

# default don't export anything
CFLAGS += -fvisibility=hidden

COMPILE.CC = $(CC) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS)
COMPILE.CXX = $(CXX) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CXXFLAGS)

#-------------------------Link---------------------------
#
#--------------------------------------------------------------------
ifeq ($(STATIC_LINK),1)
    LDFLAGS += -static
endif

#-------------------------Compile Output---------------------------
#
#--------------------------------------------------------------------
ifeq ($(UNICODE),1)
	OUTPATH += unicode.$(BUILD).$(PLATFORM)
else
	OUTPATH += $(BUILD).$(PLATFORM)
endif
OBJSPATH = $(OUTPATH)/objs

OBJECT_FILES := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE_FILES)))
OBJECT_FILES := $(addprefix $(OBJSPATH)/,$(OBJECT_FILES))
DEPENDENCE_FILES := $(OBJECT_FILES:%.o=%.d)
MKDIR = @mkdir -p $(dir $@)
#--------------------------Makefile Rules----------------------------
#
#--------------------------------------------------------------------
$(OUTPATH)/$(OUTFILE): $(OBJECT_FILES) $(STATIC_LIBS)
ifeq ($(OUTTYPE),0)
	$(CXX) -o $@ -Wl,-rpath . $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
ifeq ($(OUTTYPE),1)
	$(CXX) -o $@ -shared -fPIC -rdynamic -Wl,-rpath . $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
	@echo -e "\033[35m	AR	$(notdir $@)\033[0m"
	@$(AR) -rc $@ $^
endif
endif
	@echo make ok, output: $(OUTPATH)/$(OUTFILE)

$(OBJSPATH)/%.o : %.c
	$(MKDIR)
	@$(COMPILE.CC) -c -o $@ $<
	@echo -e "\033[35m	CC	$(notdir $@)\033[0m"
	
$(OBJSPATH)/%.o : %.cpp
	$(MKDIR)
	@$(COMPILE.CXX) -c -o $@ $<
	@echo -e "\033[35m	CXX	$(notdir $@)\033[0m"

$(OBJSPATH)/%.d: %.c
	$(MKDIR)
	@echo -e "\033[32m	CREATE	$(notdir $@)\033[0m"
	@rm -f $@; \
	 $(COMPILE.CC) -MM $(CFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$
$(OBJSPATH)/%.d: %.cpp
	$(MKDIR)
	@echo -e "\033[32m	CREATE	$(notdir $@)\033[0m"
	@rm -f $@; \
	 $(COMPILE.CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$

ifeq ($(MAKECMDGOALS), clean)
else ifeq ($(MAKECMDGOALS), debug)
else
sinclude $(DEPENDENCE_FILES)
endif

version.h : version.ver
	$(ROOT)/svnver.sh version.ver version.h

.PHONY: clean
clean:
	@echo -e "\033[35m	 rm -rf *.o  *.d $(OUTPATH)/$(OUTFILE) \033[0m"
	@rm -f $(OBJECT_FILES) $(OUTPATH)/$(OUTFILE) $(DEPENDENCE_FILES)

debug:
	echo $(OUTPATH)/$(OUTFILE)
	echo $(OBJECT_FILES)
	echo $(DEPENDENCE_FILES)