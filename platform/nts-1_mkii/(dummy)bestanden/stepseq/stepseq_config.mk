# Project name (must match directory name)
PROJECT = stepseq

# Module type (osc/modfx/delfx/revfx)
PROJECT_TYPE = modfx

# MCU Family
MCU = cortex-m7

# Source files
UCSRC = unit.cc

UCXXSRC = 

# Include the common build rules
include $(PLATFORMDIR)/common.mk
