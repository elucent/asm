# Platform properties

DETECT = rt/detect
include rt/Platform.mk

# Sources

CXX_HEADERS = $(wildcard *.h) $(wildcard arch/*.h)
CXX_SOURCES = $(wildcard *.cpp) $(wildcard arch/*.cpp)

include rt/Common.mk

# Build rules

all: debug release

$(DEBUG_MANIFEST):
	mkdir -p $(DEBUG_BUILD)/arch
	mkdir -p $(DEBUG_BUILD)/include/arch
	touch $@

$(RELEASE_MANIFEST):
	mkdir -p $(RELEASE_BUILD)/arch
	mkdir -p $(RELEASE_BUILD)/include/arch
	touch $@

$(DEBUG_BUILD)/%.d: %.cpp $(DEBUG_MANIFEST)
	$(CXX_EMIT_DEPFILE_DEBUG) $(@:.d=.o) $< $(EMIT_DEPFILE_TO) $@
	rm $(@:.d=.o)

$(RELEASE_BUILD)/%.d: %.cpp $(RELEASE_MANIFEST)
	$(CXX_EMIT_DEPFILE_RELEASE) $(@:.d=.o) $< $(EMIT_DEPFILE_TO) $@
	rm $(@:.d=.o)

include $(DEBUG_DEPFILE_INCLUDE)
include $(RELEASE_DEPFILE_INCLUDE)

$(DEBUG_BUILD)/%.o: %.cpp
	$(CXX_COMPILE_DEBUG) $@ $<

$(RELEASE_BUILD)/%.o: %.cpp
	$(CXX_COMPILE_RELEASE) $@ $<

# Products

librt.a-debug: $(DEBUG_MANIFEST)
	$(MAKE) -C rt librt.a-debug

librt.a-release: $(RELEASE_MANIFEST)
	$(MAKE) -C rt librt.a-release

libutil.a-debug: $(DEBUG_MANIFEST)
	$(MAKE) -C util libutil.a-debug

libutil.a-release: $(RELEASE_MANIFEST)
	$(MAKE) -C util libutil.a-release

libasm.a-debug: $(DEBUG_MANIFEST) $(DEBUG_DEPFILE) $(DEBUG_HEADERS) $(DEBUG_OBJS) librt.a-debug libutil.a-debug
	ar rcs $(DEBUG_BUILD)/libasm.a $(DEBUG_OBJS)

libasm.a-release: $(RELEASE_MANIFEST) $(RELEASE_DEPFILE) $(RELEASE_HEADERS) $(RELEASE_OBJS) librt.a-release libutil.a-release
	ar rcs $(RELEASE_BUILD)/libasm.a $(RELEASE_OBJS)

debug: libasm.a-debug
release: libasm.a-release

# Helpers

.PHONY: clean-debug clean-release clean

clean-debug:
	$(MAKE) -C rt clean-debug
	$(MAKE) -C util clean-debug
	rm -rf $(DEBUG_BUILD)

clean-release:
	$(MAKE) -C rt clean-release
	$(MAKE) -C util clean-release
	rm -rf $(RELEASE_BUILD)

clean:
	$(MAKE) -C rt clean
	$(MAKE) -C util clean
	rm -rf $(BUILD)
