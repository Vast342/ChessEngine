# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++20 -flto -fexceptions -Wall -Wextra -pthread
LDFLAGS := -fuse-ld=lld
CXXFLAGS += $(BUILD_CXXFLAGS)

# Root directory and evaluation file
_THIS     := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT     := $(_THIS)
EVALFILE  := $(_ROOT)/src/cn_030.nnue

CXXFLAGS += -DNetworkFile=\"$(EVALFILE)\"

# Optimization levels
DEBUG_CXXFLAGS := -g3 -O1 -DDEBUG
BUILD_CXXFLAGS := -DNDEBUG -O3

# Supported CPU architectures
CPU_ARCHS := x86-64 x86-64-v2 x86-64-v3_Magic x86-64-v3_BMI2 x86-64-v4 native

# Directories
SRC_DIR := src
BUILD_DIR := build

# Common source files
COMMON_SRCS := src/board.cpp src/globals.cpp src/move.cpp src/movegen.cpp \
               src/eval.cpp src/search.cpp src/tests.cpp src/tt.cpp \
               src/external/fathom/tbprobe.cpp
COMMON_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(COMMON_SRCS:.cpp=.o)))

# Architecture-specific sources
SRCS_x86-64 := $(COMMON_SRCS) src/magic.cpp src/uci.cpp
SRCS_x86-64-v2 := $(SRCS_x86-64)
SRCS_x86-64-v3_Magic := $(SRCS_x86-64)
SRCS_x86-64-v3_BMI2 := $(COMMON_SRCS) src/bmi2.cpp src/uci.cpp
SRCS_x86-64-v4 := $(SRCS_x86-64-v3_BMI2)
SRCS_native := $(SRCS_x86-64-v4)

# Executable names
EXE_BASE := Clarity
EXES := $(foreach arch, $(CPU_ARCHS), $(EXE_BASE)_$(arch))

# Append .exe for Windows
ifeq ($(OS),Windows_NT)
    EXES := $(addsuffix .exe, $(EXES))
endif

# Default target (builds optimized binary for native architecture)
all: $(EXE_BASE)_native

# Per-architecture build targets
define BUILD_ARCH
$(EXE_BASE)_$(1): CXXFLAGS += -march=$(2)
$(EXE_BASE)_$(1): $$(addprefix $(BUILD_DIR)/,$$(notdir $$(SRCS_$(1):.cpp=.o)))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $$@ $$^
endef

$(foreach arch, x86-64 x86-64-v2, $(eval $(call BUILD_ARCH,$(arch),$(arch))))
$(eval $(call BUILD_ARCH,x86-64-v3_Magic,x86-64-v3))
$(eval $(call BUILD_ARCH,x86-64-v3_BMI2,x86-64-v3))
$(eval $(call BUILD_ARCH,x86-64-v4,x86-64-v4))
$(eval $(call BUILD_ARCH,native,native))

# Rule to build object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: src/external/fathom/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -pthread -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $@

# Debug target
debug: CXXFLAGS += $(DEBUG_CXXFLAGS)
debug: $(EXE_BASE)_native

# Build all CPU variants for release
release: clean
	@echo "Building all CPU-optimized binaries..."
	$(MAKE) -j$(shell nproc) $(EXES)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(EXES)

# Phony targets
.PHONY: all debug clean release
