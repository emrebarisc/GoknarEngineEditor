cmake_minimum_required(VERSION 3.20)

set(APP_NAME GoknarEngineEditor)

file(STRINGS "Config/Build.ini" ENGINE_LOCATION_LINE REGEX "^EngineLocation=")
set(ENGINE_PATH "")

if(ENGINE_LOCATION_LINE)
    string(REGEX REPLACE "^EngineLocation=(.*)$" "\\1" ENGINE_PATH "${ENGINE_LOCATION_LINE}")
    string(STRIP "${ENGINE_PATH}" ENGINE_PATH)
endif()

if(ENGINE_PATH)
    file(TO_CMAKE_PATH "${ENGINE_PATH}" ENGINE_PATH)
    if(NOT IS_ABSOLUTE "${ENGINE_PATH}")
        get_filename_component(ENGINE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ENGINE_PATH}" ABSOLUTE)
    endif()
else()
    set(ENGINE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/GoknarEngine/Goknar")
endif()

get_filename_component(ENGINE_PATH "${ENGINE_PATH}" ABSOLUTE)
set(ENGINE_CONTENT_SOURCE_DIR "${ENGINE_PATH}/EngineContent")
set(GOKNAR_ENGINE_CONTENT_SOURCE_DIR "${ENGINE_CONTENT_SOURCE_DIR}" CACHE INTERNAL "Resolved engine content source directory")

set(EDITOR_ACTIVE_PROJECT_PATH "${CURRENT_PROJECT_PATH}")
if(NOT EDITOR_ACTIVE_PROJECT_PATH)
    file(STRINGS "Config/EditorConfig.ini" EDITOR_CURRENT_PROJECT_PATH_LINE REGEX "^CurrentProjectPath=")
    if(EDITOR_CURRENT_PROJECT_PATH_LINE)
        string(REGEX REPLACE "^CurrentProjectPath=(.*)$" "\\1" EDITOR_ACTIVE_PROJECT_PATH "${EDITOR_CURRENT_PROJECT_PATH_LINE}")
        string(STRIP "${EDITOR_ACTIVE_PROJECT_PATH}" EDITOR_ACTIVE_PROJECT_PATH)
    endif()
endif()

if(EDITOR_ACTIVE_PROJECT_PATH)
    file(TO_CMAKE_PATH "${EDITOR_ACTIVE_PROJECT_PATH}" EDITOR_ACTIVE_PROJECT_PATH)
    if(NOT IS_ABSOLUTE "${EDITOR_ACTIVE_PROJECT_PATH}")
        get_filename_component(EDITOR_ACTIVE_PROJECT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${EDITOR_ACTIVE_PROJECT_PATH}" ABSOLUTE)
    endif()
    get_filename_component(EDITOR_ACTIVE_PROJECT_PATH "${EDITOR_ACTIVE_PROJECT_PATH}" ABSOLUTE)
endif()

set(EDITOR_ACTIVE_PROJECT_SOURCE_ROOT "")
if(EDITOR_ACTIVE_PROJECT_PATH AND EXISTS "${EDITOR_ACTIVE_PROJECT_PATH}/Source")
    set(EDITOR_ACTIVE_PROJECT_SOURCE_ROOT "${EDITOR_ACTIVE_PROJECT_PATH}/Source")
    message(STATUS "Integrating editor dynamic factory classes from: ${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}")
endif()

project(${APP_NAME})

set(CMAKE_CXX_STANDARD 17)
option(GOKNAR_PUBLISHED_BUILD "Build the runtime to load packaged pak data." OFF)

set(SOURCE_DIR_NAME "Source")

######################################################################
##############################  FILES   ##############################
######################################################################

set(GAME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/")
set(SUBDIRS
    ${SOURCE_DIR_NAME}
    ${SOURCE_DIR_NAME}/Components
    ${SOURCE_DIR_NAME}/Controllers
    ${SOURCE_DIR_NAME}/Data
    ${SOURCE_DIR_NAME}/Lighting
    ${SOURCE_DIR_NAME}/Objects
    ${SOURCE_DIR_NAME}/Objects/DefaultScene
    ${SOURCE_DIR_NAME}/Objects/Environment
    ${SOURCE_DIR_NAME}/Objects/Portals
    ${SOURCE_DIR_NAME}/ShaderUtils
    ${SOURCE_DIR_NAME}/UI
    ${SOURCE_DIR_NAME}/UI/Panels
    ${SOURCE_DIR_NAME}/UI/Panels/ShaderEditor
    ${SOURCE_DIR_NAME}/Thirdparty
)
set(GAME_SOURCE)

foreach(SUBDIR ${SUBDIRS})
    set(COMBINED_DIR ${GAME_SOURCE_DIR}${SUBDIR})
    
    file(GLOB GROUP_FILES CONFIGURE_DEPENDS "${COMBINED_DIR}/*" )
    
    source_group(${SUBDIR} FILES ${GROUP_FILES})
    list(APPEND GAME_SOURCE ${GROUP_FILES})
endforeach()

set(EDITOR_FACTORY_GENERATOR_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/CMake/GenerateDynamicObjectFactoryRegistrar.cmake")
set(EDITOR_FACTORY_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/Generated/EditorDynamicObjectFactoryRegistrar.generated.h")
set(EDITOR_FACTORY_GENERATED_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/Generated/EditorDynamicObjectFactoryRegistrar.generated.cpp")
set(EDITOR_FACTORY_SCAN_FILES)

if(EDITOR_ACTIVE_PROJECT_SOURCE_ROOT)
    file(GLOB_RECURSE EDITOR_FACTORY_SCAN_FILES CONFIGURE_DEPENDS
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.h"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.hh"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.hpp"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.hxx"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cpp"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cc"
        "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cxx"
    )
endif()

add_custom_command(
    OUTPUT "${EDITOR_FACTORY_GENERATED_HEADER}" "${EDITOR_FACTORY_GENERATED_SOURCE}"
    COMMAND ${CMAKE_COMMAND}
        "-DOUTPUT_HEADER=${EDITOR_FACTORY_GENERATED_HEADER}"
        "-DOUTPUT_SOURCE=${EDITOR_FACTORY_GENERATED_SOURCE}"
        "-DSCAN_ROOT=${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}"
        "-DINCLUDE_ROOT=${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}"
        -P "${EDITOR_FACTORY_GENERATOR_SCRIPT}"
    DEPENDS "${EDITOR_FACTORY_GENERATOR_SCRIPT}" ${EDITOR_FACTORY_SCAN_FILES}
    VERBATIM
)

list(APPEND GAME_SOURCE "${EDITOR_FACTORY_GENERATED_SOURCE}")

if(NOT ANDROID)
    add_executable(${APP_NAME} ${GAME_SOURCE})
else()
    add_library(${APP_NAME} SHARED ${GAME_SOURCE})
    target_link_libraries(${APP_NAME} -Wl,--whole-archive cpp_android_spec -Wl,--no-whole-archive)
endif()

set_target_properties(${APP_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Output"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Output"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Output"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/Output"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/Output"
)

if(MSVC)
    set_target_properties(${APP_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/Output")
    target_compile_options(${APP_NAME} PRIVATE /MP /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(${APP_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32 OR MSVC)
    target_compile_definitions(${APP_NAME} PRIVATE GOKNAR_PLATFORM_WINDOWS)
elseif(UNIX AND NOT ANDROID)
    target_compile_definitions(${APP_NAME} PRIVATE GOKNAR_PLATFORM_UNIX)
    target_link_libraries(${APP_NAME} PRIVATE dl glfw glut GLU GL)
endif()

add_subdirectory(${ENGINE_PATH} ./Goknar)
target_compile_definitions(GOKNAR PUBLIC GOKNAR_EDITOR)

add_subdirectory(Thirdparty/Imgui/)
target_link_libraries(${APP_NAME} PUBLIC IMGUI)
target_include_directories(${APP_NAME} PUBLIC Thirdparty/Imgui/Repo)

target_link_libraries(${APP_NAME} PUBLIC GOKNAR)

target_include_directories(${APP_NAME} PUBLIC 
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/${SOURCE_DIR_NAME}
    ${EDITOR_ACTIVE_PROJECT_PATH}
    ${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}
)

target_compile_definitions(${APP_NAME} PUBLIC
    GOKNAR_BUILD_DLL
    GLFW_INCLUDE_NONE
    "$<$<CONFIG:Debug>:GOKNAR_BUILD_DEBUG>"
    "$<$<CONFIG:Debug>:GOKNAR_DEBUG>"
    "$<$<CONFIG:Debug>:GOKNAR_ENABLE_ASSERTS>"
    "$<$<CONFIG:Debug>:GOKNAR_ENABLE_LOGGING>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:GOKNAR_BUILD_RELEASE>"
)
if(GOKNAR_PUBLISHED_BUILD)
    target_compile_definitions(${APP_NAME} PUBLIC GOKNAR_PUBLISHED_BUILD=1)
endif()

if(WIN32 OR MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${APP_NAME})
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:zlib>
            $<TARGET_FILE_DIR:${APP_NAME}>
    )
endif()
