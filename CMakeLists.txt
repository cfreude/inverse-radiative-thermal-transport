cmake_minimum_required(VERSION 3.21.0)
###########################################################################
# Disable insource build

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR
		"CMake generation for this project is not allowed within the source directory!"
		"\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
		"\n "
		"\n rm CMakeCache.txt"
		"\n cd .."
		"\n mkdir cmake-make"
		"\n cd cmake-make"
		"\n cmake ../project"
	)
endif()

# look for vcpkg environment variable
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# PROJECT
project(Tamashii DESCRIPTION "Tamashii" LANGUAGES CXX C)
# SET
set(APP tamashii)
set(LIB_ENGINE tamashii_engine)
set(LIB_GUI tamashii_gui)
set(LIB_RENDERER_VK tamashii_renderer_vk)
set(LIB_CUDA_HELPER tamashii_cuda_helper)
set(LIB_IMPL tamashii_implementations)
set(LIB_RVK rvk)
set(ASSETS assets)

set(FRAMEWORK_EXTERNAL_FOLDER "External")
set(FRAMEWORK_TARGET_FOLDER "Framework")
set(FRAMEWORK_EXAMPLE_FOLDER "Examples")

set(CUDA_HELPER off)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(INCLUDE_DIR "${SOURCE_DIR}/include")
set(RVK_INCLUDE_DIR "${SOURCE_DIR}/src/rvk/include")
set(EXTERNAL_DIR "${SOURCE_DIR}/external")

option(BUILD_TAMASHII "Build tamashii" ON)
option(USE_AVX "Activate AVX instructions" ON)
option(USE_AVX2 "Activate AVX2 instructions" ON)

# COMPILER
if(WIN32)
	# c++ 17 and multi core compilation
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc /std:c++17 /MP /Qpar")
	if(USE_AVX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
	endif()
	if(USE_AVX2)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	endif()
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
elseif(APPLE)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_EXTENSIONS OFF)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wwrite-strings -x objective-c++")
elseif(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wwrite-strings")
	if(USE_AVX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
	endif()
	if(USE_AVX2)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
	endif()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
  	message("IPO is not supported: ${output}")
endif()

# DEFINES
# GLM requires this to enable the experimental extension headers (glm/gtx/*)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
#add_definitions(-DVK_NO_PROTOTYPES)
if(WIN32)
	add_definitions(
		-DNOMINMAX
		-D_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING
		-D_CRT_SECURE_NO_WARNINGS
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
		-DVK_USE_PLATFORM_WIN32_KHR
		-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
	)
elseif(APPLE)
	add_definitions(
		-DVK_USE_PLATFORM_MACOS_MVK
		-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
	)
elseif(UNIX)
	add_definitions(
		-DVK_USE_PLATFORM_XCB_KHR
		-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
	)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# OUTPUT DIR
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "")

# External Builds
message(STATUS "<spdlog>")
add_subdirectory("${EXTERNAL_DIR}/spdlog" EXCLUDE_FROM_ALL)
message(STATUS "</spdlog>")
set_target_properties(spdlog PROPERTIES FOLDER ${FRAMEWORK_EXTERNAL_FOLDER})

# glslang
# Build glslang from source. The prebuilt *.lib files referenced by src/rvk are not
# present in this checkout, and this glslang version no longer ships SPVRemapper/OGLCompiler.
message(STATUS "<glslang>")
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${EXTERNAL_DIR}/glslang" EXCLUDE_FROM_ALL)
message(STATUS "</glslang>")

add_subdirectory(src)
add_subdirectory(assets)

# ===============================#
# Unset unneeded cache variables #
# ===============================#

# unset(CMAKE_BUILD_TYPE CACHE)
# unset(CMAKE_CONFIGURATION_TYPES CACHE)
# unset(CMAKE_INSTALL_PREFIX CACHE)

unset(VCPKG_APPLOCAL_DEPS CACHE)
unset(VCPKG_INSTALLED_DIR CACHE)
unset(VCPKG_MANIFEST_DIR CACHE)
unset(VCPKG_MANIFEST_MODE CACHE)
unset(VCPKG_PREFER_SYSTEM_LIBS CACHE)
unset(VCPKG_SETUP_CMAKE_PROGRAM_PATH CACHE)
unset(VCPKG_TARGET_TRIPLET CACHE)
unset(X_VCPKG_APPLOCAL_DEPS_INSTALL CACHE)
unset(X_VCPKG_APPLOCAL_DEPS_SERIALIZED CACHE)
unset(Z_VCPKG_BUILTIN_POWERSHELL_PATH CACHE)
unset(Z_VCPKG_PWSH_PATH CACHE)
unset(_VCPKG_INSTALLED_DIR CACHE)

unset(SPDLOG_BUILD_ALL CACHE)
unset(SPDLOG_BUILD_SHARED CACHE)
unset(SPDLOG_ENABLE_PCH CACHE)
unset(SPDLOG_BUILD_PIC CACHE)
unset(SPDLOG_BUILD_EXAMPLE CACHE)
unset(SPDLOG_BUILD_EXAMPLE_HO CACHE)
unset(SPDLOG_BUILD_TESTS CACHE)
unset(SPDLOG_BUILD_TESTS_HO CACHE)
unset(SPDLOG_BUILD_BENCH CACHE)
unset(SPDLOG_SANITIZE_ADDRESS CACHE)
unset(SPDLOG_BUILD_WARNINGS CACHE)
unset(SPDLOG_INSTALL CACHE)
unset(SPDLOG_USE_STD_FORMAT CACHE)
unset(SPDLOG_FMT_EXTERNAL CACHE)
unset(SPDLOG_FMT_EXTERNAL_HO CACHE)
unset(SPDLOG_NO_EXCEPTIONS CACHE)
unset(SPDLOG_WCHAR_SUPPORT CACHE)
unset(SPDLOG_WCHAR_FILENAMES CACHE)
unset(SPDLOG_CLOCK_COARSE CACHE)
unset(SPDLOG_PREVENT_CHILD_FD CACHE)
unset(SPDLOG_NO_THREAD_ID CACHE)
unset(SPDLOG_NO_TLS CACHE)
unset(SPDLOG_DISABLE_DEFAULT_LOGGER CACHE)
unset(SPDLOG_NO_ATOMIC_LEVELS CACHE)
unset(SPDLOG_TIDY CACHE)