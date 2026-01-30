PROJECT = brever_inside_out
PROJECT_TYPE = revfx

UCSRC = header.c
UCXXSRC = unit.cc

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

