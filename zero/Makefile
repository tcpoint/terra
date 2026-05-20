# Project Name
TARGET = zero

OPT = -Og
# Sources
CPP_SOURCES = zero.cpp zflanger.cpp zlfo.cpp

# Library Locations
LIBDAISY_DIR = ../../DaisyExamples/libDaisy
DAISYSP_DIR = ../../DaisyExamples/DaisySP

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

C_INCLUDES += -I../Terrarium \
	-I../../DaisyExamples/DaisySP/Source/Utility
