# Included from the native muimaster GNUmakefile, sharing its build root.
ROOT := $(abspath ../../..)
COMPONENT_BUILD := $(abspath $(BUILD))
FLEXCAT ?= flexcat
PLUGIN_DIRS := $(shell $(PYTHON) $(ROOT)/tools/components.py list)
PLUGIN_TARGETS := $(addprefix plugin-,$(PLUGIN_DIRS))
EXAMPLE_NAMES := HelloZune HGroup VGroup VHGroup VHGroup2 HVGroup Notify
EXAMPLES := $(addprefix $(BUILD)/Examples/,$(EXAMPLE_NAMES))

.PHONY: core prefs plugins catalogs sdk examples $(PLUGIN_TARGETS)
all: prefs plugins catalogs sdk examples
prefs: $(GENERATED)
	$(MAKE) -C ../../prefs/Zune -f GNUmakefile BUILD=$(COMPONENT_BUILD)
plugins: $(PLUGIN_TARGETS)
$(PLUGIN_TARGETS):
	$(MAKE) -C $(ROOT)/workbench/classes/zune/$(patsubst plugin-%,%,$@) \
	  -f $(ROOT)/tools/plugin.mk BUILD=$(COMPONENT_BUILD) \
	  OBJDIR=$(COMPONENT_BUILD)/plugins/$(patsubst plugin-%,%,$@) \
	  PLUGIN_MAKEFILE=$(ROOT)/tools/plugin.mk FC=$(FLEXCAT) \
	  PLUGIN_CFLAGS='$(CFLAGS)' LDFLAGS_EXTRA='$(LDFLAGS)'
catalogs:
	$(PYTHON) $(ROOT)/tools/components.py catalogs --build $(COMPONENT_BUILD) --flexcat $(FLEXCAT)
sdk: $(GENERATED)
	$(PYTHON) $(ROOT)/tools/components.py sdk --build $(COMPONENT_BUILD)
check: check-components
.PHONY: check-components
check-components: prefs plugins catalogs sdk examples
	$(PYTHON) $(ROOT)/tests/check_components.py $(COMPONENT_BUILD)
examples: $(EXAMPLES)
$(BUILD)/Examples/%: tutorial/examples/%.c tutorial/examples/muizunesupport.h $(GENERATED)
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) -DNO_INLINE_STDARG \
	  -I$(BUILD)/include -Iinclude -Wl,-u,___stkinit -o $@ $< -lmui
