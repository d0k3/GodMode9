PROCESSOR := ARM11

TARGET := $(shell basename $(CURDIR))

SOURCE := source
BUILD  := build

SUBARCH := -D$(PROCESSOR) -march=armv6k -mtune=mpcore -marm -mfloat-abi=hard -mfpu=vfpv2 -mtp=soft
INCDIRS := source
INCLUDE := $(foreach dir,$(INCDIRS),-I"$(shell pwd)/$(dir)")

ASFLAGS += $(SUBARCH) $(INCLUDE)
CFLAGS  += $(SUBARCH) $(INCLUDE) -flto
LDFLAGS += $(SUBARCH) -Wl,--use-blx,-Map,$(TARGET).map -flto

include ../Makefile.common
include ../Makefile.build
