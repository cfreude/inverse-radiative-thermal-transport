cmake_minimum_required(VERSION 3.9)

file(GLOB_RECURSE SOURCES "*.h" "*.hpp" "*.c" "*.cpp")
foreach(FILE ${SOURCES}) 
   # Get the directory of the source file
   get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
   # Remove common directory prefix to make the group
   string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
   # Make sure we are using windows slashes
   string(REPLACE "/" "\\" GROUP "${GROUP}")
   source_group("${GROUP}" FILES "${FILE}")
endforeach()

# Include Eigen headers
include_directories("${EXTERNAL_DIR}/eigen")

###########
### APP ###
###########

set(APP "thermal_renderer_app")

# exe
if(WIN32)
	add_executable(${APP} WIN32 ${SOURCES})	
   # set working dir and startup project
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
elseif(APPLE)
else(UNIX)
	add_executable(${APP} ${SOURCES})	
endif()

if (MSVC)	
	SET(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
	SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -ggdb3")
endif()


# DEPENDENCIES
#project(${APP} LANGUAGES CXX)
project(${APP} LANGUAGES CXX)
#find_package(Eigen3 REQUIRED NO_MODULE)
#target_link_libraries(${APP} PRIVATE Eigen3::Eigen)
# OPENMP (for Eigen)
find_package(OpenMP REQUIRED)
target_link_libraries(${APP} PRIVATE OpenMP::OpenMP_CXX)
# spdlog
#find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${APP} PRIVATE spdlog::spdlog)
#cuda
#find_package(CUDA 11.0 REQUIRED)
#target_link_libraries(${PROJECT_NAME} ${CUDA_LIBRARIES})
# OpenCL
#find_package( OpenCL REQUIRED )
#include_directories(${OPENCL_INCLUDE_DIR} )
#target_link_libraries(${APP} PRIVATE OpenCL::OpenCL)
# tamashii
include_directories("${INCLUDE_DIR}")
add_dependencies(${APP} ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})
target_link_libraries(${APP} PRIVATE ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})

install(TARGETS ${APP} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)


###########
### LIB ###
###########

set(APP "thermal_renderer_lib")

include (GenerateExportHeader)

#add_library(${APP} ${LIB_TYPE} ${SOURCES})
add_library(${APP} SHARED ${SOURCES})

# lib
if(WIN32)
   # set working dir and startup project
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
elseif(APPLE)
else(UNIX)
	add_executable(${APP} ${SOURCES})	
endif()

if (MSVC)	
	SET(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
	SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -ggdb3")
endif()

GENERATE_EXPORT_HEADER( ${APP}
             BASE_NAME ${APP}
             EXPORT_MACRO_NAME ${APP}_EXPORT
             EXPORT_FILE_NAME ${APP}_Export.h
             STATIC_DEFINE ${APP}_BUILT_AS_STATIC
)

# DEPENDENCIES
project(${APP} LANGUAGES CXX)
#find_package(Eigen3 REQUIRED NO_MODULE)
#target_link_libraries(${APP} PRIVATE Eigen3::Eigen)
# OPENMP (for Eigen)
find_package(OpenMP REQUIRED)
target_link_libraries(${APP} PRIVATE OpenMP::OpenMP_CXX)
# spdlog
#find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${APP} PRIVATE spdlog::spdlog)
#cuda
#find_package(CUDA 11.0 REQUIRED)
#target_link_libraries(${PROJECT_NAME} ${CUDA_LIBRARIES})
# OpenCL
#find_package( OpenCL REQUIRED )
#include_directories(${OPENCL_INCLUDE_DIR} )
#target_link_libraries(${APP} PRIVATE OpenCL::OpenCL)
# tamashii
include_directories("${INCLUDE_DIR}")
add_dependencies(${APP} ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})
target_link_libraries(${APP} PRIVATE ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})

target_compile_definitions(${APP} PUBLIC thermal_renderer_lib_EXPORTS)
target_compile_definitions(${APP} PUBLIC DISABLE_GUI)
target_include_directories(${APP} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})


################
### LIB TEST ###
################

set(APP "thermal_renderer_lib_test")

# exe
if(WIN32)
	add_executable(${APP} WIN32 ${SOURCES})	
   # set working dir and startup project
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
elseif(APPLE)
else(UNIX)
	add_executable(${APP} ${SOURCES})	
endif()

if (MSVC)	
	SET(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
	SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -ggdb3")
endif()


# DEPENDENCIES
project(${APP} LANGUAGES CXX)
# OPENMP (for Eigen)
#find_package(Eigen3 REQUIRED NO_MODULE)
#target_link_libraries(${APP} PRIVATE Eigen3::Eigen)
find_package(OpenMP REQUIRED)
target_link_libraries(${APP} PRIVATE OpenMP::OpenMP_CXX)
# spdlog
#find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${APP} PRIVATE spdlog::spdlog)
#cuda
#find_package(CUDA 11.0 REQUIRED)
#target_link_libraries(${PROJECT_NAME} ${CUDA_LIBRARIES})
# OpenCL
#find_package( OpenCL REQUIRED )
#include_directories(${OPENCL_INCLUDE_DIR} )
#target_link_libraries(${APP} PRIVATE OpenCL::OpenCL)
# tamashii
include_directories("${INCLUDE_DIR}")
add_dependencies(${APP} ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})
target_link_libraries(${APP} PRIVATE ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})

add_dependencies(${APP} "thermal_renderer_lib")
target_link_libraries(${APP} PUBLIC "thermal_renderer_lib")

target_compile_definitions(${APP} PUBLIC ENABLE_LIB_TEST)
target_include_directories(${APP} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})


###########
### CLI ###
###########

set(APP "thermal_renderer_cli")

# exe
if(WIN32)
	add_executable(${APP} WIN32 ${SOURCES})	
   # set working dir and startup project
   set_target_properties(${APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
elseif(APPLE)
else(UNIX)
	add_executable(${APP} ${SOURCES})	
endif()

if (MSVC)	
	SET(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
	SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -ggdb3")
endif()


# DEPENDENCIES
project(${APP} LANGUAGES CXX)
# OPENMP (for Eigen)
#find_package(Eigen3 REQUIRED NO_MODULE)
#target_link_libraries(${APP} PRIVATE Eigen3::Eigen)
find_package(OpenMP REQUIRED)
target_link_libraries(${APP} PRIVATE OpenMP::OpenMP_CXX)
# spdlog
#find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${APP} PRIVATE spdlog::spdlog)
# CLI11
target_include_directories(${APP} PRIVATE "${EXTERNAL_DIR}/CLI11")
include_directories("${EXTERNAL_DIR}/CLI11/include")
#cuda
#find_package(CUDA 11.0 REQUIRED)
#target_link_libraries(${PROJECT_NAME} ${CUDA_LIBRARIES})
# OpenCL
#find_package( OpenCL REQUIRED )
#include_directories(${OPENCL_INCLUDE_DIR} )
#target_link_libraries(${APP} PRIVATE OpenCL::OpenCL)
# tamashii
include_directories("${INCLUDE_DIR}")
add_dependencies(${APP} ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})
target_link_libraries(${APP} PRIVATE ${LIB_ENGINE} ${LIB_RENDERER_VK} ${LIB_IMPL} ${LIB_GUI})

#add_dependencies(${APP} "thermal_renderer_lib")
#target_link_libraries(${APP} PUBLIC "thermal_renderer_lib")

target_include_directories(${APP} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(${APP} PUBLIC ENABLE_CLI)
target_include_directories(${APP} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})


install(TARGETS ${APP} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
