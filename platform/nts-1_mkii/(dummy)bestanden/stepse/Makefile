# #############################################################################
# Makefile for NTS-1 mkII STEPSEQ modulation effect
# Based on Korg logue SDK template
# #############################################################################

PROJECT := stepseq
PROJECT_TYPE := modfx

# Platform
PLATFORM := nts-1_mkii

# Toolchain (adjust paths if needed)
TOOLS_DIR := /usr/local/arm-none-eabi
GCC_TOOLS := $(TOOLS_DIR)/bin

# Include SDK paths
PLATFORMDIR := $(CURDIR)/../..
SDK_DIR := $(PLATFORMDIR)/..

# Source files
UCSRC := unit.cc
UCXXSRC := 
UINCDIR := 
UDEFS := 

# Include platform makefile
include $(PLATFORMDIR)/platform.mk

# Build target
all: build

.PHONY: build clean

build:
	@echo "Building STEPSEQ modulation effect..."
	@echo "Note: Full SDK required for compilation"
	@echo ""
	@echo "To build with official SDK:"
	@echo "1. Place this directory in: logue-sdk/platform/nts-1_mkii/stepseq/"
	@echo "2. Run: make"
	@echo ""
	@echo "Files ready for SDK build:"
	@ls -1 *.c *.cc *.json *.mk 2>/dev/null || true

clean:
	@echo "Cleaning build artifacts..."
	@rm -f *.o *.elf *.bin *.hex *.nts1mkiiunit
