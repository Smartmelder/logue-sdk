PROJECT = dub_beast
PROJECT_TYPE = delfx

UCSRC = header.c
UCXXSRC = unit.cc

UINCDIR =
ULIBDIR =
ULIBS =
UDEFS =

# Explicitly set EXTDIR for Docker builds
EXTDIR ?= $(PROJECT_ROOT)/../../ext

