file(GLOB_RECURSE SOURCES "src/*.h" "src/*.hpp" "src/*.c" "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp" "include/*.c" "include/*.cpp" "include/*.inl")

# GROUPING
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})

# EXECUTABLE
add_library(${LIB_RVK} STATIC ${SOURCES} ${HEADERS})
add_library(${LIB_RVK}::${LIB_RVK} ALIAS ${LIB_RVK})
set_target_properties(${LIB_RVK} PROPERTIES FOLDER ${FRAMEWORK_TARGET_FOLDER})

# DEPS
# glslang
# glslang is built from source as a subproject (see root CMakeLists.txt). Its
# PUBLIC include directory is external/glslang, which resolves both
# <glslang/Public/ShaderLang.h> and <SPIRV/GlslangToSpv.h>.
target_link_libraries(${LIB_RVK} PRIVATE glslang::glslang)
target_link_libraries(${LIB_RVK} PRIVATE glslang-default-resource-limits)
if(UNIX)
   find_package(Threads REQUIRED)
   target_link_libraries(${LIB_RVK} PRIVATE Threads::Threads)
   target_link_libraries(${LIB_RVK} PRIVATE  ${CMAKE_DL_LIBS})
endif()

# dxc
# The full checkout bundles DXC under external/dxc, but that directory is absent in this
# checkout. The Vulkan SDK (a hard requirement) ships DXC, so use it for the Windows build.
if(WIN32)
   target_include_directories(${LIB_RVK} PRIVATE "$ENV{VULKAN_SDK}/Include/")
   set(DXC_BIN_DIR "$ENV{VULKAN_SDK}/Bin/")
   add_library(DXC_LIB SHARED IMPORTED GLOBAL)
   set_property(TARGET DXC_LIB PROPERTY IMPORTED_IMPLIB "$ENV{VULKAN_SDK}/Lib/dxcompiler.lib")
   set_property(TARGET DXC_LIB PROPERTY IMPORTED_LOCATION "$ENV{VULKAN_SDK}/Bin/dxcompiler.dll")
   target_link_libraries(${LIB_RVK} PRIVATE debug DXC_LIB)
   target_link_libraries(${LIB_RVK} PRIVATE optimized DXC_LIB)
elseif(APPLE)
   set(DXC_BIN_DIR "${EXTERNAL_DIR}/dxc/bin/macos-x64/release/")
   target_link_libraries(${LIB_RVK} PRIVATE debug "${DXC_BIN_DIR}/libdxcompiler.3.7.dylib")
   target_link_libraries(${LIB_RVK} PRIVATE optimized "${DXC_BIN_DIR}/libdxcompiler.3.7.dylib")
else(UNIX)
   set(DXC_BIN_DIR "${EXTERNAL_DIR}/dxc/bin/linux-x64/release/")
   target_link_libraries(${LIB_RVK} PRIVATE debug "${DXC_BIN_DIR}/libdxcompiler.so")
   target_link_libraries(${LIB_RVK} PRIVATE optimized "${DXC_BIN_DIR}/libdxcompiler.so")
endif()

# INCLUDES
find_package(Vulkan REQUIRED)
target_include_directories(${LIB_RVK} PUBLIC "${Vulkan_INCLUDE_DIR}" "${SOURCE_DIR}/src/rvk/include")

# COMPILER
target_compile_definitions(${LIB_RVK} PRIVATE _R_VK_EXPORT_DLL)

