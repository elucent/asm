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
	$(CXX_EMIT_DEPFILE_DEBUG) $(@:.d=.o) $< $(FREESTANDING_FLAGS) $(EMIT_DEPFILE_TO) $@
	rm $(@:.d=.o)

$(RELEASE_BUILD)/%.d: %.cpp $(RELEASE_MANIFEST)
	$(CXX_EMIT_DEPFILE_RELEASE) $(@:.d=.o) $< $(FREESTANDING_FLAGS) $(EMIT_DEPFILE_TO) $@
	rm $(@:.d=.o)

include $(DEBUG_DEPFILE_INCLUDE)
include $(RELEASE_DEPFILE_INCLUDE)

$(DEBUG_BUILD)/%.o: %.cpp $(DEBUG_MANIFEST)
	$(CXX_COMPILE_DEBUG) $@ $< $(FREESTANDING_FLAGS)

$(RELEASE_BUILD)/%.o: %.cpp $(RELEASE_MANIFEST)
	$(CXX_COMPILE_RELEASE) $@ $< $(FREESTANDING_FLAGS)

# Products

$(DEBUG_BUILD)/librt.a: $(DEBUG_MANIFEST)
	$(MAKE) -C rt librt.a-debug
	cp rt/$(DEBUG_BUILD)/librt.a $@
	cp -r rt/$(DEBUG_BUILD)/include $(DEBUG_BUILD)/include/rt

$(RELEASE_BUILD)/librt.a: $(RELEASE_MANIFEST)
	$(MAKE) -C rt librt.a-release
	cp rt/$(RELEASE_BUILD)/librt.a $@
	cp -r rt/$(RELEASE_BUILD)/include $(RELEASE_BUILD)/include/rt

$(DEBUG_BUILD)/libutil.a: $(DEBUG_MANIFEST)
	$(MAKE) -C util libutil.a-debug
	cp util/$(DEBUG_BUILD)/libutil.a $@
	cp -r util/$(DEBUG_BUILD)/include $(DEBUG_BUILD)/include/util

$(RELEASE_BUILD)/libutil.a: $(RELEASE_MANIFEST)
	$(MAKE) -C util libutil.a-release
	cp util/$(RELEASE_BUILD)/libutil.a $@
	cp -r util/$(RELEASE_BUILD)/include $(RELEASE_BUILD)/include/util

debug-libs: $(DEBUG_BUILD)/librt.a $(DEBUG_BUILD)/libutil.a
release-libs: $(RELEASE_BUILD)/librt.a $(RELEASE_BUILD)/libutil.a

libasm.a-debug: $(DEBUG_MANIFEST) $(DEBUG_DEPFILE) $(DEBUG_HEADERS) $(DEBUG_OBJS)
	ar rcs $(DEBUG_BUILD)/libasm.a $(DEBUG_OBJS)

libasm.a-release: $(RELEASE_MANIFEST) $(RELEASE_DEPFILE) $(RELEASE_HEADERS) $(RELEASE_OBJS)
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

deps: $(DEBUG_DEPFILE) $(RELEASE_DEPFILE)
	$(MAKE) -C rt deps
	$(MAKE) -C util deps
