CC  = clang
CXX = clang++
GUEST_ISA = riscv64

BUILD_DIR = $(abspath ./build)
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
# --------------------------------------------
# spike
# --------------------------------------------
SPIKE_CFLAGS    = -O3 -include $(abspath .)/cosim-def.h
SPIKE_CXXFLAGS  = -O3 -include $(abspath .)/cosim-def.h -Wno-c99-designator
SPIKE_LDFLAGS   =

SPIKE_CONFIGURE_FLAGS = \
    --with-boost=no\
    --with-boost-asio=no \
    --with-boost-regex=no \
    CC=$(CC) \
    CXX=$(CXX)  \
    CFLAGS="$(SPIKE_CFLAGS)" \
    CXXFLAGS="$(SPIKE_CXXFLAGS)" \
    LDFLAGS="$(SPIKE_LDFLAGS)"

SPIKE_PATH = $(abspath ..)
SPIKE_SRCS = $(shell find $(SPIKE_PATH) -maxdepth 2 -name '*.cc') \
             $(shell find $(SPIKE_PATH) -maxdepth 3 -name '*.h')

SPIKE_MAKEFILE = $(BUILD_DIR)/Makefile
$(SPIKE_MAKEFILE): $(SPIKE_PATH)/configure | $(BUILD_DIR)
	cd $(@D) && $(abspath $(SPIKE_PATH))/configure $(SPIKE_CONFIGURE_FLAGS)

SPIKE_TARGET = $(BUILD_DIR)/spike
$(SPIKE_TARGET): $(SPIKE_MAKEFILE) $(SPIKE_SRCS)
	$(MAKE) -s -C $(BUILD_DIR)

# --------------------------------------------
# spike-so
# --------------------------------------------
# Dependencies for building the cosim dynamic library
COSIM_SOURCES = $(shell find . -maxdepth 1 -name '*.cc') $(CXX_UTILS)
COSIM_HEADERS = $(shell find . -maxdepth 1 -name '*.h')
# We need some utilities from spike
CXX_UTILS = $(BUILD_DIR)/cosim_utils.cc
$(CXX_UTILS): $(SPIKE_PATH)/spike_main/spike.cc
	cp $< $(CXX_UTILS)
	@sed -i 's/static //g' $@
	@sed -i 's/int main/static int spike_main/g' $@

LIBS = libriscv.a libdisasm.a libsoftfloat.a libfesvr.a libfdt.a
LIBS_PATH   = $(addprefix $(BUILD_DIR)/, $(LIBS))

INCS = fesvr riscv disasm customext fdt softfloat spike_main spike_dasm
INCS_PATH = -I$(SPIKE_PATH) -I$(BUILD_DIR) $(addprefix -I$(SPIKE_PATH)/, $(INCS))

CXXFLAGS = $(SPIKE_CXXFLAGS) --std=c++17 -shared -fPIC

TARGET = $(BUILD_DIR)/$(GUEST_ISA)-spike-so
$(TARGET): $(SPIKE_TARGET) $(COSIM_SOURCES) $(COSIM_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCS_PATH) $(COSIM_SOURCES) $(LIBS_PATH) -o $@

# --------------------------------------------
# others
# --------------------------------------------
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.DEFAULT_GOAL = $(TARGET)

