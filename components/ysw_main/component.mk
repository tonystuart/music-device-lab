
# MM_V01 1 = OSHPark board with vs1053 synth
# MM_V02 2 = JLCPCB board with internal DAC and board level mods
# MM_V03 3 = JLCPCB board with fixed schematic (never manufactured)
# MM_V04 4 = Wire-wrapped board with CS4344 DAC
# MM_V05 5 = Ai Thinker esp32-audio-kit with AC101 Codec (wire-wrapped)
# MM_V06 6 = Ai Thinker esp32-audio-kit with AC101 Codec (pcb)

CFLAGS += \
	-DMM_V01=1 \
	-DMM_V02=2 \
	-DMM_V04=4 \
	-DMM_V05=5 \
	-DMM_V06=6 \
	-DMM_VERSION=MM_V06

