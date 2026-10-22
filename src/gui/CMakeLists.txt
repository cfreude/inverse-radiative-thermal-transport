set(SOURCES "imgui_gui.cpp")
set(HEADERS "${INCLUDE_DIR}/tamashii/gui/imgui_gui.hpp")

# includes all of the imgui frontend needed
# backend is in renderer_vk
set(EXTERNALS
   # imgui
   "${EXTERNAL_DIR}/imgui/imgui.h"
   "${EXTERNAL_DIR}/imgui/imgui_internal.h"
   "${EXTERNAL_DIR}/imgui/imgui.cpp"
   "${EXTERNAL_DIR}/imgui/imgui_draw.cpp"
   "${EXTERNAL_DIR}/imgui/imgui_tables.cpp"
   "${EXTERNAL_DIR}/imgui/imgui_widgets.cpp"
   # imGuizmo
   "${EXTERNAL_DIR}/ImGuizmo/ImGuizmo.cpp"
)

if(WIN32)
   list(APPEND EXTERNALS "${EXTERNAL_DIR}/imgui/backends/imgui_impl_win32.h" "${EXTERNAL_DIR}/imgui/backends/imgui_impl_win32.cpp")
elseif(APPLE)
   list(APPEND EXTERNALS "${EXTERNAL_DIR}/imgui/backends/imgui_impl_osx.h" "${EXTERNAL_DIR}/imgui/backends/imgui_impl_osx.cpp")
elseif(UNIX)
   list(APPEND HEADERS "${INCLUDE_DIR}/tamashii/gui/imgui_impl_x11.h")
   list(APPEND SOURCES "imgui_impl_x11.cpp")
endif()

# GROUPING
source_group(TREE "${INCLUDE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${EXTERNAL_DIR}" PREFIX "External Files" FILES ${EXTERNALS})

# EXECUTABLE with imgui
add_library(${LIB_GUI} STATIC ${SOURCES} ${HEADERS} ${EXTERNALS})
add_library(${APP}::gui ALIAS ${LIB_GUI})
set_target_properties(${LIB_GUI} PROPERTIES FOLDER ${FRAMEWORK_TARGET_FOLDER})

# INCLUDE
target_include_directories(${LIB_GUI} 
   PUBLIC 
   "${EXTERNAL_DIR}/imgui" "${EXTERNAL_DIR}/imgui/backends"
   "${EXTERNAL_DIR}/ImGuizmo"
   "${EXTERNAL_DIR}/imoguizmo"
   "${EXTERNAL_DIR}/glm"
   "${EXTERNAL_DIR}/spdlog/include"
)

# DEPS
add_dependencies(${LIB_GUI} spdlog::spdlog)
target_link_libraries(${LIB_GUI} PUBLIC spdlog::spdlog)

if(WIN32)
elseif(APPLE)
else(UNIX)
   find_package(X11 REQUIRED)
   target_link_libraries(${LIB_GUI} PRIVATE "-lxcb-xkb -lxcb-cursor -lxcb-keysyms")
endif()

# COMPILE
set_target_properties(${LIB_GUI} PROPERTIES LINKER_LANGUAGE CXX)
