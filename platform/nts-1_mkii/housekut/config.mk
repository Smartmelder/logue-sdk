# HOUSEKUT Configuration

PROJECT := housekut

PROJECT_TYPE := osc

# Sources

UCSRC := header.c

UCXXSRC := unit.cc

UASMSRC := 

UASMXSRC := 

# Include directories

UINCDIR := 

# Library directories

ULIBDIR := 

# Defines

UDEFS := 

# Optimization

OPT := -g -Os -mlittle-endian

OPT += -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant -fcheck-new

OPT += -fno-exceptions

