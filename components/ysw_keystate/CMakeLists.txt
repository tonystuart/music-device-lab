idf_component_register(
  SRCS
    ysw_keystate.c
  INCLUDE_DIRS
    include
  REQUIRES
    ysw_bus
    ysw_common
    ysw_event
    ysw_heap
  PRIV_REQUIRES
)
