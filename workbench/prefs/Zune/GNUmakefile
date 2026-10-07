# Native GCC preferences executable.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
CROSS ?= m68k-amigaos-
CC := $(CROSS)gcc
BUILD ?= ../../../build/muimaster
CPUFLAGS ?= -m68000 -msoft-float
CFLAGS ?= -std=gnu17 -Os -g -Wall -Wno-pointer-sign
MASTER := ../../libs/muimaster
CPPFLAGS += -DNO_INLINE_STDARG -I$(BUILD)/include \
  -I$(BUILD)/prefs -I$(BUILD) -Iinclude -I$(MASTER)/include -include clib/alib_protos.h
SOURCES := prefswindowp.c zune.c listview.c zunestuff.c windowp.c buttonsp.c groupsp.c \
  cyclesp.c slidersp.c scrollbarsp.c listviewsp.c stringsp.c specialp.c \
  navigationp.c framesp.c locale.c systemp.c amigasupport.c
OBJECTS := $(BUILD)/prefs/prefswindow-base.o $(addprefix $(BUILD)/prefs/,$(SOURCES:.c=.o))
.PHONY: all
all: $(BUILD)/Prefs/Zune
$(BUILD)/Prefs/Zune: $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul $(LDFLAGS) -Wl,-u,___stkinit -o $@ $^ -lmui -ldebug
$(BUILD)/prefs/%.o: %.c GNUmakefile
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
-include $(OBJECTS:.o=.d)

FLEXCAT ?= flexcat
$(BUILD)/prefs/strings.h: catalogs/zune.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=../../../tools/flexcat/CatComp_h.sd
$(BUILD)/prefs/prefswindow_strings.h: ../../classes/zune/prefswindow/catalogs/prefswindow.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=../../../tools/flexcat/CatComp_h.sd
$(OBJECTS): $(BUILD)/prefs/strings.h $(BUILD)/prefs/prefswindow_strings.h
$(BUILD)/prefs/prefswindow-base.o: ../../classes/zune/prefswindow/prefswindow.c
	$(CC) $(CPUFLAGS) -noixemul $(CPPFLAGS) $(CFLAGS) -c $< -o $@
