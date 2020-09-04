#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := player

include $(IDF_PATH)/make/project.mk

CFLAGS += -DYSW_MAIN_SYNTH_MODEL=3 -DYSW_MAIN_DISPLAY_MODEL=2 -DA2DP_SUPPORT=1

display-macros:
	echo CC=$(CC)
	echo CFLAGS=$(CFLAGS)
	$(CC) -dM -E - < /dev/null | sort

