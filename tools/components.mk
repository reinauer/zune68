# Included from the native muimaster GNUmakefile, sharing its build root.
ROOT := $(abspath ../../..)
COMPONENT_BUILD := $(abspath $(BUILD))
FLEXCAT ?= flexcat
PLUGIN_DIRS := $(shell $(PYTHON) $(ROOT)/tools/components.py list)
PLUGIN_TARGETS := $(addprefix plugin-,$(PLUGIN_DIRS))
EXAMPLE_NAMES := HelloZune HGroup VGroup VHGroup VHGroup2 HVGroup Notify TheBarDemo
EXAMPLES := $(addprefix $(BUILD)/Examples/,$(EXAMPLE_NAMES))

.PHONY: core prefs plugins catalogs sdk examples $(PLUGIN_TARGETS)
all: prefs plugins catalogs sdk examples
prefs: $(GENERATED)
	$(MAKE) -C ../../prefs/Zune -f GNUmakefile BUILD=$(COMPONENT_BUILD)
plugins: $(PLUGIN_TARGETS) imageclasses
.PHONY: imageclasses
imageclasses: $(GENERATED)
	$(MAKE) -C $(ROOT)/tools/imageclass BUILD=$(COMPONENT_BUILD) \
	  CFLAGS='$(CFLAGS)'

$(PLUGIN_TARGETS):
	$(MAKE) -C $(ROOT)/workbench/classes/zune/$(patsubst thebar/mcc-virtual,thebar/mcc,$(patsubst plugin-%,%,$@)) \
	  -f $(ROOT)/tools/plugin.mk BUILD=$(COMPONENT_BUILD) \
	  OBJDIR=$(COMPONENT_BUILD)/plugins/$(patsubst plugin-%,%,$@) \
	  PLUGIN_MAKEFILE=$(ROOT)/tools/plugin.mk FC=$(FLEXCAT) \
	  PLUGIN_CFLAGS='$(CFLAGS)' LDFLAGS_EXTRA='$(LDFLAGS)' \
	  $(if $(filter plugin-thebar/mcc-virtual,$@),PLUGIN_DEFINES=-DVIRTUAL PLUGIN_NAME=TheBarVirt.mcc) \
	  $(if $(filter plugin-thebar/%,$@),PLUGIN_ADAPTER=$(ROOT)/tools/thebar/runtime.h)
catalogs:
	$(PYTHON) $(ROOT)/tools/components.py catalogs --build $(COMPONENT_BUILD) --flexcat $(FLEXCAT)
sdk: $(GENERATED)
	$(PYTHON) $(ROOT)/tools/components.py sdk --build $(COMPONENT_BUILD)
check: check-components
.PHONY: check-components
check-components: prefs plugins catalogs sdk examples
	$(PYTHON) $(ROOT)/tests/check_components.py $(COMPONENT_BUILD)
	$(PYTHON) $(ROOT)/tests/check_prefs_cancel.py $(COMPONENT_BUILD)/Prefs/Zune
	$(PYTHON) $(ROOT)/tests/check_thebar.py $(COMPONENT_BUILD)
	$(PYTHON) $(ROOT)/tests/check_bzip2.py
	$(PYTHON) $(ROOT)/tests/check_pixmap.py $(COMPONENT_BUILD)/Libs/MUI/Pixmap.mui
	$(PYTHON) $(ROOT)/tests/check_pixmap_render.py $(COMPONENT_BUILD)/Libs/MUI/Pixmap.mui
examples: $(EXAMPLES)
$(BUILD)/Examples/%: tutorial/examples/%.c tutorial/examples/muizunesupport.h $(GENERATED)
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) -DNO_INLINE_STDARG \
	  -I$(BUILD)/include -Iinclude -Wl,-u,___stkinit -o $@ $< -lmui

$(BUILD)/Examples/TheBarDemo: $(ROOT)/tests/native/thebar_demo.c $(GENERATED) \
  $(ROOT)/workbench/classes/zune/thebar/include/mui/TheBar_mcc.h \
  $(ROOT)/workbench/classes/zune/thebar/include/mui/Toolbar_mcc.h
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) -DNO_INLINE_STDARG \
	  -I$(BUILD)/include -Iinclude -I$(ROOT)/workbench/classes/zune/thebar/include \
	  -Wl,-u,___stkinit -o $@ $< -lmui
