file(GLOB_RECURSE SOURCES "*.h" "*.hpp" "*.c" "*.cpp")
file(GLOB_RECURSE HEADERS "")

# GROUPING
source_group(TREE "${INCLUDE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})

if(WIN32)
   add_executable(${APP} WIN32 ${SOURCES} ${HEADERS})
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")   
   set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APP})

   # copy dlls
   add_custom_command(TARGET ${APP} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${APP}> $<TARGET_FILE_DIR:${APP}>
      COMMAND_EXPAND_LISTS
   )
elseif(APPLE)
   add_executable(${APP} MACOSX_BUNDLE ${SOURCES})
   set_target_properties(${APP} PROPERTIES XCODE_GENERATE_SCHEME TRUE CMAKE_XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")  
   set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY XCODE_STARTUP_PROJECT ${APP})
else(UNIX)
   add_executable(${APP} ${SOURCES} ${HEADERS})
endif()

# DEPS
target_link_libraries(${APP} PRIVATE tamashii::engine tamashii::vkrenderer tamashii::implementations)
add_dependencies(${APP} tamashii::engine tamashii::vkrenderer tamashii::implementations)

if(RENDERER_OGL)
   # opengl
   find_package(OpenGL REQUIRED)
   target_link_libraries(${APP} PRIVATE OpenGL::GL)
   target_link_libraries(${APP} PRIVATE ${LIB_RENDERER_OGL})
   add_dependencies(${APP} ${LIB_RENDERER_OGL})
endif()

# INSTALL
if(WIN32)
   install(TARGETS ${APP} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
   install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION bin/assets)
   install(FILES $<TARGET_RUNTIME_DLLS:${APP}> DESTINATION bin)
elseif(APPLE)
elseif(UNIX)
   install(TARGETS ${APP} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
   install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION bin/assets)
endif()
