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

DEFINES += OS_LINUX

#--------------------------------Compile-----------------------------
#
#--------------------------------------------------------------------
AR := $(CROSS)ar
CC := $(CROSS)gcc
CXX := $(CROSS)g++
CFLAGS += -Wall
CXXFLAGS += -Wall

ifeq ($(RELEASE),1)
	CFLAGS += -Wall -O2
	CXXFLAGS += $(CFLAGS)
	DEFINES += NDEBUG
else
	CFLAGS += -g -Wall
	CXXFLAGS += $(CFLAGS)
	DEFINES += DEBUG _DEBUG
endif

CFLAGS += -fvisibility=hidden # default don't export anything

COMPILE.CC = $(CC) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS)
COMPILE.CXX = $(CXX) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS)

#--------------------------------VERSION------------------------------
# make NOVERSION=1
#--------------------------------------------------------------------
ifneq ($(OUTTYPE),2)
	ifeq ($(NOVERSION),1)
		OBJS_VER := 
	else
		OBJS_VER := poversion.o
	endif
endif

#-------------------------Compile Template---------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
ifeq ($(UNICODE),1)
	OUTPATH += unicode.$(BUILD).$(PLATFORM)
else
	OUTPATH += $(BUILD).$(PLATFORM)
endif

TMP_OUTPATH_VAR := $(shell if [ ! -d $(OUTPATH) ]; then \
															mkdir $(OUTPATH); \
														fi;)

OBJECT_FILES := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE_FILES)))
OBJECT_FILES := $(foreach file,$(OBJECT_FILES),$(OUTPATH)/$(notdir $(file)))

#--------------------------Makefile Rules----------------------------
#
# generate make rules
#--------------------------------------------------------------------
#GCC_RULE_FILE:=gcc.rule
#TMP_GCC_RULES_CLEAR := $(shell if [ -f $(GCC_RULE_FILE) ]; then rm -f $(GCC_RULE_FILE); fi;)
#TMP_GCC_RULES := $(foreach item,$(sort $(dir $(OBJECT_FILES))),$(shell \
#													echo '$(OUTPATH)/%.o: $(item)%.cpp' >> $(GCC_RULE_FILE); \
#													echo '	$(CXX) -c -o $$@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CXXFLAGS) -MMD $$^' >> $(GCC_RULE_FILE); \
#													echo '$(OUTPATH)/%.o: $(item)%.c' >> $(GCC_RULE_FILE); \
#													echo '	$(CC) -c -o $$@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS) -MMD $$^' >> $(GCC_RULE_FILE); \
#									 ))

#TMP_GCC_RULES := $(shell if [ -f $(GCC_RULE_FILE) ]; then rm -f $(GCC_RULE_FILE); fi; \
#												for item in $(OBJECT_FILES); do echo $(item) >> gcc.rule; done; \
#												echo $(DIRS) >> gcc.rule; \
#												for item in $(DIRS); do \
#													echo '$(OUTPATH)/%.o: $(item)/%.cpp' >> $(GCC_RULE_FILE); \
#													echo '	$(CXX) -c -o $$@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CXXFLAGS) -MMD $$^' >> $(GCC_RULE_FILE); \
#													echo '$(OUTPATH)/%.o: $(item)/%.c' >> $(GCC_RULE_FILE); \
#													echo '	$(CC) -c -o $$@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS) -MMD $$^' >> $(GCC_RULE_FILE); \
#												done \
#											)

$(OUTPATH)/$(OUTFILE): $(OBJECT_FILES) $(STATIC_LIBS) $(OBJS_VER)
ifeq ($(OUTTYPE),0)
	$(CXX) -o $@ -Wl,-rpath . $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
ifeq ($(OUTTYPE),1)
	$(CXX) -o $@ -shared -fpic -rdynamic -Wl,-rpath . $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
	$(AR) -rc $@ $^
endif
endif
	@echo make ok, output: $(OUTPATH)/$(OUTFILE)

#%.d :
#	@echo $@
#	@set -e; \
#	if [ "$(filter $(patsubst %.d, \%%.cpp, $(notdir $@)), $(SOURCE_FILES))" == "" ]; then \
#		$(COMPILE.CC) -MM $(filter $(patsubst %.d, \%%.c, $(notdir $@)), $(SOURCE_FILES)) > $(OUTPATH)/$(notdir $@.$$$$); \
#		sed 's,\($*\)\.o[ :]*,\1.o $> : ,g' < $(OUTPATH)/$(notdir $@.$$$$) > $(OUTPATH)/$(notdir $@); \
#		rm -f $(OUTPATH)/$(notdir $@.$$$$); \
#	else \
#		$(COMPILE.CXX) -MM $(filter $(patsubst %.d, \%%.cpp, $(notdir $@)), $(SOURCE_FILES)) > $(OUTPATH)/$(notdir $@.$$$$); \
#		sed 's,\($*\)\.o[ :]*,\1.o $> : ,g' < $(OUTPATH)/$(notdir $@.$$$$) > $(OUTPATH)/$(notdir $@); \
#		rm -f $(OUTPATH)/$(notdir $@.$$$$); \
#	fi

GEN_GCC_RULES := $(shell $(ROOT)/gccrule.sh "$(OUTPATH)" "$(SOURCE_FILES)" "$(COMPILE.CC)" "$(COMPILE.CXX)")
include $(OBJECT_FILES:.o=.d)

#%.o : %.d
#	$(CC) -c -o $@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS) -MMD $<
#	$(COMPILE.CC) -c -o $@ $(filter $(patsubst %.o, \%%.c, $(@F)), $(SOURCE_FILES)) -MMD;
#	$(CXX) -c -o $@ $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS) -MMD $<
#	$(COMPILE.CXX) -c -o $@ $(filter $(patsubst %.o, \%%.cpp, $(@F), $(SOURCE_FILES)) -MMD;

poversion.o : poversion.ver
	$(ROOT)/svnver.sh poversion.ver poversion.c
	$(CC) -c poversion.c

.PHONY: clean print
clean:
	rm -f $(OBJECT_FILES) $(OUTPATH)/$(OUTFILE) $(patsubst %.o,%.d,$(OBJECT_FILES)) $(OBJS_VER)

print:
	@echo Output: $(OUTPATH)/$(OUTFILE)
	@echo ---------------------------------Include---------------------------------
	@echo $(INCLUDES)
	@echo " "
	@echo ---------------------------------Source---------------------------------
	@echo $(SOURCE_FILES)
	@echo " "
	@echo ---------------------------------Objects---------------------------------
	@echo $(OBJECT_FILES)
