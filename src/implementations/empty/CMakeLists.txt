file(GLOB_RECURSE SOURCES "*.h" "*.hpp" "*.c" "*.cpp")

# GROUPING
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})

set(APP "empty")
# EXECUTABLE
if(WIN32)
	add_executable(${APP} WIN32 ${SOURCES})	
   # set working dir and startup project
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

   # copy dlls
   add_custom_command(TARGET ${APP} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${APP}> $<TARGET_FILE_DIR:${APP}>
      COMMAND_EXPAND_LISTS
   )
elseif(APPLE)
   add_executable(${APP} MACOSX_BUNDLE ${SOURCES})
   set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY XCODE_STARTUP_PROJECT ${APP})
else(UNIX)
	add_executable(${APP} ${SOURCES})	
endif()
set_target_properties(${APP} PROPERTIES FOLDER ${FRAMEWORK_EXAMPLE_FOLDER})

# INCLUDES
include_directories("${INCLUDE_DIR}")
# DEPENDENCIES
# tamashii
target_link_libraries(${APP} PRIVATE ${LIB_ENGINE} ${LIB_GUI} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_RVK})
add_dependencies(${APP} ${LIB_ENGINE} ${LIB_GUI} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_RVK})

