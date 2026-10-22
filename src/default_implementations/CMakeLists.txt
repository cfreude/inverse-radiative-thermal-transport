file(GLOB_RECURSE SOURCES "*.h" "*.hpp" "*.c" "*.cpp") 
file(GLOB_RECURSE HEADERS "${INCLUDE_DIR}/tamashii/implementations/*.h" "${INCLUDE_DIR}/tamashii/implementations/*.hpp" 
"${INCLUDE_DIR}/tamashii/implementations/*.c" "${INCLUDE_DIR}/tamashii/implementations/*.cpp")

# GROUPING
source_group(TREE "${INCLUDE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})

# EXECUTABLE
add_library(${LIB_IMPL} STATIC ${SOURCES} ${HEADERS})
add_library(${APP}::implementations ALIAS ${LIB_IMPL})
set_target_properties(${LIB_IMPL} PROPERTIES FOLDER ${FRAMEWORK_TARGET_FOLDER})

# DEPS
# tamashii
add_dependencies(${LIB_IMPL} ${LIB_GUI} ${LIB_RVK})
target_link_libraries(${LIB_IMPL} PRIVATE ${LIB_GUI} ${LIB_RVK})

# COMPILE
set_target_properties(${LIB_IMPL} PROPERTIES LINKER_LANGUAGE CXX)
