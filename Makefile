# Platform properties

INCLUDE_DIRS = . ..
DETECT = rt/build/detect
include rt/build/Platform.mk

# Sources

include rt/build/Common.mk

RT_PATH := rt
include rt/build/Products.mk

UTIL_PATH := util
include build/Products.mk

ASM_PATH := .
include build/Products.mk

DEBUG_OBJS := $(RT_DEBUG_OBJS) $(UTIL_DEBUG_OBJS) $(ASM_DEBUG_OBJS)
DEBUG_HEADERS := $(RT_DEBUG_HEADERS) $(UTIL_DEBUG_HEADERS) $(ASM_DEBUG_HEADERS)
DEBUG_DEPFILES := $(RT_DEBUG_DEPFILES) $(UTIL_DEBUG_DEPFILES) $(ASM_DEBUG_DEPFILES)
RELEASE_OBJS := $(RT_RELEASE_OBJS) $(UTIL_RELEASE_OBJS) $(ASM_RELEASE_OBJS)
RELEASE_HEADERS := $(RT_RELEASE_HEADERS) $(UTIL_RELEASE_HEADERS) $(ASM_RELEASE_HEADERS)
RELEASE_DEPFILES := $(RT_RELEASE_DEPFILES) $(UTIL_RELEASE_DEPFILES) $(ASM_RELEASE_DEPFILES)

include rt/build/CommonProducts.mk

# Build rules

all: debug release

$(DEBUG_MANIFEST): $(BUILD_CONFIG)
	mkdir -p $(RT_DIRS_DEBUG) $(UTIL_DIRS_DEBUG) $(ASM_DIRS_DEBUG)
	touch $@

$(RELEASE_MANIFEST): $(BUILD_CONFIG)
	mkdir -p $(RT_DIRS_RELEASE) $(UTIL_DIRS_RELEASE) $(ASM_DIRS_RELEASE)
	touch $@

include $(DEBUG_DEPFILE_INCLUDE)
include $(RELEASE_DEPFILE_INCLUDE)

# Products

libasm.a-debug: $(DEBUG_MANIFEST) $(DEBUG_HEADERS) $(DEBUG_OBJS)
	cat $(DEBUG_DEPFILES) > $(DEBUG_DEPFILE)
	ar rcs $(DEBUG_BUILD)/libasm.a $(DEBUG_OBJS)

libasm.so-debug: $(DEBUG_MANIFEST) $(DEBUG_HEADERS) $(DEBUG_OBJS)
	cat $(DEBUG_DEPFILES) > $(DEBUG_DEPFILE)
	$(CXX_LINK_SHARED_DEBUG) $(DEBUG_BUILD)/libasm.so $<

libasm.a-release: $(RELEASE_MANIFEST) $(RELEASE_HEADERS) $(RELEASE_OBJS)
	cat $(RELEASE_DEPFILES) > $(RELEASE_DEPFILE)
	ar -rcs $(RELEASE_BUILD)/libasm.a $(RELEASE_OBJS)

libasm.so-release: $(RELEASE_MANIFEST) $(RELEASE_HEADERS)  $(RELEASE_OBJS)
	cat $(RELEASE_DEPFILES) > $(RELEASE_DEPFILE)
	$(CXX_LINK_SHARED_RELEASE) $(RELEASE_BUILD)/libasm.so $<

debug: libasm.a-debug libasm.so-debug
release: libasm.a-release libasm.so-release

# Helpers

.PHONY: clean-debug clean-release clean

clean-debug:
	rm -rf $(DEBUG_BUILD)

clean-release:
	rm -rf $(RELEASE_BUILD)

clean:
	rm -rf $(BUILD)

deps: $(DEBUG_DEPFILE) $(RELEASE_DEPFILE)
