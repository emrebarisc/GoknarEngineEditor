cmake_minimum_required(VERSION 3.20)

set(APP_NAME GoknarEngineEditor)

project(${APP_NAME})

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_DIR_NAME "Source")

######################################################################
##############################	FILES	##############################
######################################################################

set(GAME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/")
set(SUBDIRS
    ${SOURCE_DIR_NAME}
    ${SOURCE_DIR_NAME}/Components
    ${SOURCE_DIR_NAME}/Controllers
    ${SOURCE_DIR_NAME}/Data
    ${SOURCE_DIR_NAME}/Lighting
    ${SOURCE_DIR_NAME}/Objects
    ${SOURCE_DIR_NAME}/Objects/Environment
    ${SOURCE_DIR_NAME}/Objects/Portals
    ${SOURCE_DIR_NAME}/UI
    ${SOURCE_DIR_NAME}/Thirdparty
)
set(GAME_SOURCE)

foreach(SUBDIR ${SUBDIRS})
	set(COMBINED_DIR ${GAME_SOURCE_DIR}${SUBDIR})
	file(GLOB GROUP_FILES "${COMBINED_DIR}/*" )
	source_group(${SUBDIR} FILES ${GROUP_FILES})
	list(APPEND GAME_SOURCE ${GROUP_FILES})
endforeach()

######################################################################

set(ENGINE_PATH ${CMAKE_BINARY_DIR}/../GoknarEngine/Goknar)

if(WIN32 OR MSVC)
    add_compile_definitions(GOKNAR_PLATFORM_WINDOWS)
elseif(UNIX)
    add_compile_definitions(GOKNAR_PLATFORM_UNIX)
endif()

if(UNIX)
    SET(GCC_COVERAGE_LINK_FLAGS "-ldl -lglfw -lglut -lGLU -lGL")
    add_definitions(${GCC_COVERAGE_LINK_FLAGS})
endif()

if(NOT ANDROID)
    add_executable(${APP_NAME} ${GAME_SOURCE})
else()
    add_library(${APP_NAME} SHARED ${GAME_SOURCE})
    target_link_libraries(${APP_NAME} -Wl,--whole-archive cpp_android_spec -Wl,--no-whole-archive)
endif()

add_subdirectory(${ENGINE_PATH} ./Goknar)

add_subdirectory(Thirdparty/Imgui/)
target_link_libraries(${APP_NAME} PUBLIC IMGUI)
target_include_directories(${APP_NAME} PUBLIC 
    Thirdparty/Imgui/Repo
)

target_link_libraries(${APP_NAME} PUBLIC GOKNAR)

target_include_directories(${APP_NAME} PUBLIC 
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/${SOURCE_DIR_NAME})

add_compile_definitions(GOKNAR_BUILD_DLL GOKNAR_ENABLE_ASSERTS GLFW_INCLUDE_NONE)

file(COPY "Content" DESTINATION ${CMAKE_BINARY_DIR}/)