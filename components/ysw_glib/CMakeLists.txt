idf_component_register(
  SRCS
    ysw_glib.c
    ysw_pthread.c
  INCLUDE_DIRS
    include
    include/glib
  REQUIRES
    ysw_heap
  PRIV_REQUIRES
)
