file(GLOB_RECURSE SOURCES "*.h" "*.hpp" "*.c" "*.cpp") 
file(GLOB_RECURSE HEADERS "${INCLUDE_DIR}/tamashii/renderer_vk/*.h" "${INCLUDE_DIR}/tamashii/renderer_vk/*.hpp")

# includes all of the imgui backend needed
# frontend is in tamashii_gui
file(GLOB EXTERNALS
   # imgui
   "${EXTERNAL_DIR}/imgui/backends/imgui_impl_vulkan.h"
   "${EXTERNAL_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
)

# GROUPING
source_group(TREE "${INCLUDE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${EXTERNAL_DIR}" PREFIX "External Files" FILES ${EXTERNALS})

# EXECUTABLE
add_library(${LIB_RENDERER_VK} STATIC ${SOURCES} ${HEADERS} ${EXTERNALS})
add_library(${APP}::vkrenderer ALIAS ${LIB_RENDERER_VK})
set_target_properties(${LIB_RENDERER_VK} PROPERTIES FOLDER ${FRAMEWORK_TARGET_FOLDER})
target_include_directories(${LIB_RENDERER_VK} PRIVATE "${EXTERNAL_DIR}/imgui/backends")

# DEPS
# tamashii
add_dependencies(${LIB_RENDERER_VK} ${LIB_ENGINE} ${LIB_GUI} ${LIB_RVK})
target_link_libraries(${LIB_RENDERER_VK} PUBLIC ${LIB_ENGINE} ${LIB_GUI} ${LIB_RVK})

# COMPILE
set_target_properties(${LIB_RENDERER_VK} PROPERTIES LINKER_LANGUAGE CXX)
