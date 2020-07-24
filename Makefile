#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := player

include $(IDF_PATH)/make/project.mk

CFLAGS += -DYSW_MAIN_SYNTH_MODEL=2 -DYSW_MAIN_DISPLAY_MODEL=2
