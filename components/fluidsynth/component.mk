# Add additional source directories, one per line, see lvgl/component.mk

# Add the following to CFLAGS to enable Bluetooth A2DP support
# -DA2DP_SUPPORT=1 \

CFLAGS += \
	-Wno-unknown-pragmas \
	-Wno-error=pointer-sign \
	-DI2S_SUPPORT

COMPONENT_SRCDIRS := src src/bindings src/drivers src/midi src/rvoice src/sfloader src/synth src/utils
COMPONENT_ADD_INCLUDEDIRS := . include $(COMPONENT_SRCDIRS)
