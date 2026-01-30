PROJECT = gabber_kutje
PROJECT_TYPE = osc

UCSRC = header.c
UCXXSRC = gabber.cc unit.cc

UINCDIR =
ULIBDIR =
ULIBS =
UDEFS =

# Explicitly set EXTDIR for Docker builds
EXTDIR ?= $(PROJECT_ROOT)/../../ext
# Fallback to absolute path if relative doesn't work
ifeq ($(wildcard $(EXTDIR)/CMSIS/CMSIS/Include/arm_math.h),)
  EXTDIR := /workspace/platform/ext
endif

