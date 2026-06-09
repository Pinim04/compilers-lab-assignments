project(assignment1)

#===============================================================================
# Algebraic Identity pass
#===============================================================================
add_library(AlgIdentity SHARED AlgIdentity.cpp)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(AlgIdentity
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set_target_properties(AlgIdentity PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#===============================================================================
# Strenght Reduction pass
#===============h================================================================
add_library(StrengthRed SHARED StrengthRed.cpp)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(StrengthRed
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set_target_properties(StrengthRed PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#===============================================================================
# Multi Instruction pass
#===============h================================================================
add_library(MultiInst SHARED MultiInst.cpp)

# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(MultiInst
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set_target_properties(MultiInst PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
