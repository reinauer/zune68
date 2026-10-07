# Reuse each imported project's OS3 source list, not its in-tree build rules.
.DEFAULT_GOAL := zune68-plugin
.DELETE_ON_ERROR:
override OS := os3
override DEPFILE := /dev/null
include Makefile

CROSS ?= m68k-amigaos-
CPUFLAGS ?= -m68000 -msoft-float
PLUGIN_CFLAGS ?= -std=gnu17 -Os -g -Wall -Wno-pointer-sign
CC := $(CROSS)gcc
# SDI's saveds is unnecessary for this absolute-data build.
override CFLAGS := $(CPUFLAGS) -noixemul $(PLUGIN_CFLAGS) \
  -fno-common -fno-builtin -fno-strict-aliasing -D__amigaos3__ \
  -DNO_INLINE_STDARG -U__saveds -D__saveds= -DZUNE68_GCC_NATIVE \
  -include $(dir $(PLUGIN_MAKEFILE))plugin/runtime.h \
  -I$(OBJDIR) -I. -I../include -I../include/mui \
  -I../../include -I../mcp -I../nlistview_mcc -I../nlistviews_mcp
PLUGIN_OBJECTS := $(addprefix $(OBJDIR)/,$(LOBJS) $(filter-out vastubs.o,$(COBJS)))
PLUGIN_OUTPUT := $(BUILD)/Libs/MUI/$(notdir $(TARGET))
.PHONY: zune68-plugin
zune68-plugin: $(PLUGIN_OUTPUT)
RUNTIME_DIR := $(dir $(PLUGIN_MAKEFILE))plugin
RUNTIME_OBJECTS := $(addprefix $(OBJDIR)/runtime/,format.o strings.o superattrs.o)
$(RUNTIME_OBJECTS): $(PLUGIN_MAKEFILE)
$(OBJDIR)/runtime/strings.o: $(RUNTIME_DIR)/runtime.h
$(OBJDIR)/runtime/%.o: $(RUNTIME_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul $(PLUGIN_CFLAGS) -fno-builtin -c $< -o $@
$(OBJDIR)/runtime.a: $(RUNTIME_OBJECTS)
	$(CROSS)ar rcs $@ $^
$(PLUGIN_OUTPUT): $(PLUGIN_OBJECTS) $(OBJDIR)/runtime.a
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib $(LDFLAGS_EXTRA) -o $@ $^ \
	  -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lm -lamiga -ldebug \
	  -Wl,--end-group -Wl,-Map,$@.map

$(PLUGIN_OBJECTS): $(PLUGIN_MAKEFILE)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

ifneq ($(filter locale.o,$(COBJS)),)
POT := $(firstword $(wildcard locale/*.pot))
$(OBJDIR)/locale.c: $(POT) C_h.sd C_c.sd
	@mkdir -p $(@D)
	$(FC) $(POT) $(OBJDIR)/locale.h=C_h.sd $(OBJDIR)/locale.c=C_c.sd
$(OBJDIR)/locale.h: $(OBJDIR)/locale.c
	@test -f $@ || $(FC) $(POT) $@=C_h.sd
$(PLUGIN_OBJECTS): $(OBJDIR)/locale.h
$(OBJDIR)/locale.o: $(OBJDIR)/locale.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
endif
-include $(PLUGIN_OBJECTS:.o=.d)
