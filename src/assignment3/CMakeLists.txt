project(assignment3)

#===============================================================================
# Code Motion pass
#===============================================================================
add_library(CodeMotion SHARED CodeMotion.cpp)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(CodeMotion
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set_target_properties(CodeMotion PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)