# Platform properties

INCLUDE_DIRS = . ..
DETECT = rt/build/detect
include rt/build/Platform.mk

# Sources

include rt/build/Common.mk

RT_PATH := rt
include rt/build/Products.mk

UTIL_PATH := util
include util/build/Products.mk

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

test-asm-debug: $(DEBUG_MANIFEST) $(DEBUG_HEADERS) $(DEBUG_OBJS) $(TEST_HARNESS_DEBUG_OBJS) $(RT_ENTRY_DEBUG_OBJS) $(ASM_TEST_DEBUG_OBJS)
	cat $(DEBUG_DEPFILES) > $(DEBUG_DEPFILE)
	$(LINK_TESTS) $(DEBUG_BUILD)/test-asm.cpp $(ASM_TEST_DEBUG_OBJS)
	$(CXX_LINK_EXECUTABLE_DEBUG) $(DEBUG_BUILD)/test-asm $(DEBUG_BUILD)/test-asm.cpp $(DEBUG_OBJS) $(TEST_HARNESS_DEBUG_OBJS) $(RT_ENTRY_DEBUG_OBJS) $(ASM_TEST_DEBUG_OBJS)

test-asm-release: $(RELEASE_MANIFEST) $(RELEASE_HEADERS) $(RELEASE_OBJS) $(TEST_HARNESS_RELEASE_OBJS) $(RT_ENTRY_RELEASE_OBJS) $(ASM_TEST_RELEASE_OBJS)
	cat $(RELEASE_DEPFILES) > $(RELEASE_DEPFILE)
	$(LINK_TESTS) $(RELEASE_BUILD)/test-asm.cpp $(ASM_TEST_RELEASE_OBJS)
	$(CXX_LINK_EXECUTABLE_RELEASE) $(RELEASE_BUILD)/test-asm $(RELEASE_BUILD)/test-asm.cpp $(RELEASE_OBJS) $(TEST_HARNESS_RELEASE_OBJS) $(RT_ENTRY_RELEASE_OBJS) $(ASM_TEST_RELEASE_OBJS)

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
