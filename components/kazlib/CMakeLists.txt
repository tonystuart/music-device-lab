idf_component_register(SRCS 
                          ./dict.c
                          ./except.c
                          ./hash.c
                          ./list.c
                          ./sfx.c
                       INCLUDE_DIRS
                          .
                       REQUIRES 
                       PRIV_REQUIRES )

target_compile_options(${COMPONENT_LIB} PRIVATE -DNDEBUG)
