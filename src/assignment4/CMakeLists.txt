project(assignment4)

#===============================================================================
# Loop Fusion pass
#===============================================================================
add_library(LoopFusion SHARED LoopFusion.cpp)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(LoopFusion
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set_target_properties(LoopFusion PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)