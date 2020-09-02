# Add additional source directories, one per line, see lvgl/component.mk

CFLAGS += -Wno-unknown-pragmas -Wno-error=pointer-sign

COMPONENT_SRCDIRS := src src/bindings src/drivers src/midi src/rvoice src/sfloader src/synth src/utils
COMPONENT_ADD_INCLUDEDIRS := . include $(COMPONENT_SRCDIRS)
