# SOURCES
file(GLOB_RECURSE SOURCES "*.hpp" "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "platform/(win32|macos|unix)")
if(WIN32)
   file (GLOB_RECURSE SOURCES_WIN32 "platform/win32/*.*")
   list (APPEND SOURCES ${SOURCES_WIN32})
elseif(APPLE)
   file (GLOB_RECURSE SOURCES_MACOS "platform/macos/*.*")
   list (APPEND SOURCES ${SOURCES_MACOS})
else(UNIX)
   file (GLOB_RECURSE SOURCES_UNIX "platform/unix/*.*")
   list (APPEND SOURCES ${SOURCES_UNIX})
endif()
# HEADERS
file(GLOB_RECURSE HEADERS "${INCLUDE_DIR}/tamashii/public.hpp" "${INCLUDE_DIR}/tamashii/engine/*.*")
# EXTERNALS
file(GLOB EXTERNALS
   # miniz
   "${EXTERNAL_DIR}/tinyexr/deps/miniz/miniz.c"
   # MikkTSpace
   "${EXTERNAL_DIR}/MikkTSpace/mikktspace.c"
)

# GROUPING
source_group(TREE "${INCLUDE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${EXTERNAL_DIR}" PREFIX "External Files" FILES ${EXTERNALS})

# EXECUTABLE
add_library(${LIB_ENGINE} STATIC ${SOURCES} ${HEADERS} ${EXTERNALS})
add_library(${APP}::engine ALIAS ${LIB_ENGINE})
set_target_properties(${LIB_ENGINE} PROPERTIES FOLDER ${FRAMEWORK_TARGET_FOLDER})

# INCLUDE
target_include_directories(${LIB_ENGINE} 
   PRIVATE 
   "${EXTERNAL_DIR}/tinyply/source"
   "${EXTERNAL_DIR}/tinyobjloader"
   "${EXTERNAL_DIR}/tinyies"
   "${EXTERNAL_DIR}/tinyldt"
   "${EXTERNAL_DIR}/tinyexr"
   "${EXTERNAL_DIR}/tinyexr/deps/miniz"
   "${EXTERNAL_DIR}/MikkTSpace"
   "${EXTERNAL_DIR}/tinygltf"
   "${EXTERNAL_DIR}/nlohmann_json/include"
   "${EXTERNAL_DIR}/stb"
   PUBLIC
   "${EXTERNAL_DIR}/glm"
   "${EXTERNAL_DIR}/spdlog/include"
)

# DEPS
add_dependencies(${LIB_ENGINE} ${LIB_GUI} spdlog::spdlog)
target_link_libraries(${LIB_ENGINE} PUBLIC ${LIB_GUI} spdlog::spdlog)

if(WIN32)
   target_link_libraries(${LIB_ENGINE} 
      PRIVATE "winmm.lib" "dinput8.lib" "dxguid.lib" "Shlwapi.lib"
   )
elseif(APPLE)
   target_link_libraries(${LIB_ENGINE} 
      PRIVATE "-framework Foundation" "-framework AppKit" "-framework GameController"
   )
else(UNIX)
   find_package(X11 REQUIRED)
   find_package(Threads REQUIRED)
   target_link_libraries(${LIB_ENGINE} PRIVATE "-lxcb -lxcb-util -lxcb-xkb -lxcb-xfixes -lxcb-cursor -lxcb-keysyms -lncurses")
   target_link_libraries(${LIB_ENGINE} PRIVATE Threads::Threads)
endif()

# COMPILE
set_target_properties(${LIB_ENGINE} PROPERTIES LINKER_LANGUAGE CXX)
