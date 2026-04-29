cmake_minimum_required(VERSION 3.20)

# Read ProjectName from Config/Build.ini
file(STRINGS "Config/Build.ini" PROJECT_NAME_LINE REGEX "^ProjectName=")
if(PROJECT_NAME_LINE)
    string(REGEX REPLACE "^ProjectName=(.*)$" "\\1" APP_NAME "${PROJECT_NAME_LINE}")
    string(STRIP "${APP_NAME}" APP_NAME)
else()
    set(APP_NAME "ExampleProject")
endif()

file(STRINGS "Config/Build.ini" ENGINE_LOCATION_LINE REGEX "^EngineLocation=")
if(ENGINE_LOCATION_LINE)
    string(REGEX REPLACE "^EngineLocation=(.*)$" "\\1" ENGINE_PATH "${ENGINE_LOCATION_LINE}")
    string(STRIP "${ENGINE_PATH}" ENGINE_PATH)
    file(TO_CMAKE_PATH "${ENGINE_PATH}" ENGINE_PATH)
    if(NOT IS_ABSOLUTE "${ENGINE_PATH}")
        get_filename_component(ENGINE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ENGINE_PATH}" ABSOLUTE)
    endif()
else()
    get_filename_component(ENGINE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Goknar" ABSOLUTE)
endif()

get_filename_component(ENGINE_PATH "${ENGINE_PATH}" ABSOLUTE)
set(ENGINE_CONTENT_SOURCE_DIR "${ENGINE_PATH}/EngineContent")
set(GOKNAR_ENGINE_CONTENT_SOURCE_DIR "${ENGINE_CONTENT_SOURCE_DIR}" CACHE INTERNAL "Resolved engine content source directory")

project(${APP_NAME})

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_DIR_NAME "Source")

######################################################################
##############################  FILES   ##############################
######################################################################

set(GAME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/")
set(SUBDIRS
    ${SOURCE_DIR_NAME}
    ${SOURCE_DIR_NAME}/Characters
    ${SOURCE_DIR_NAME}/Components
    ${SOURCE_DIR_NAME}/Controllers
    ${SOURCE_DIR_NAME}/Data
    ${SOURCE_DIR_NAME}/Objects
    ${SOURCE_DIR_NAME}/UI
    ${SOURCE_DIR_NAME}/UI/Panels
)
set(GAME_SOURCE)

foreach(SUBDIR ${SUBDIRS})
    set(COMBINED_DIR ${GAME_SOURCE_DIR}${SUBDIR})
    
    # Automatically detects new files without needing to touch CMakeLists.txt
    file(GLOB GROUP_FILES CONFIGURE_DEPENDS "${COMBINED_DIR}/*" )
    
    source_group(${SUBDIR} FILES ${GROUP_FILES})
    list(APPEND GAME_SOURCE ${GROUP_FILES})
endforeach()

######################################################################

# Setup the Executable or Library first so we can attach properties to it
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
    set_target_properties(${APP_NAME} PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/Output"
    )
endif()

# MSVC-Specific compiler flags for speed and strictness
if(MSVC)
    target_compile_options(${APP_NAME} PRIVATE
        /MP           # Enables multi-processor compilation
        /W4           # High warning level
        /permissive-  # Enforces strict C++ standard conformance
        /Zc:__cplusplus # Forces MSVC to report the correct C++ version to macros
    )

    target_compile_definitions(${APP_NAME} PRIVATE
        NOMINMAX                  # Prevents Windows.h from hijacking std::min and std::max
        WIN32_LEAN_AND_MEAN       # Excludes rarely-used Windows headers to speed up compilation
        _CRT_SECURE_NO_WARNINGS   # Disables warnings for standard C functions
    )
endif()

if(WIN32 OR MSVC)
    target_compile_definitions(${APP_NAME} PRIVATE GOKNAR_PLATFORM_WINDOWS)
elseif(UNIX AND NOT ANDROID)
    target_compile_definitions(${APP_NAME} PRIVATE GOKNAR_PLATFORM_UNIX)
    
    target_link_libraries(${APP_NAME} PRIVATE dl glfw glut GLU GL)
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
    ${PROJECT_SOURCE_DIR}/${SOURCE_DIR_NAME}
)

target_compile_definitions(${APP_NAME} PUBLIC GOKNAR_BUILD_DLL GOKNAR_ENABLE_ASSERTS GLFW_INCLUDE_NONE)

if(WIN32 OR MSVC)
    # Force Visual Studio Startup Project using the variable
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${APP_NAME})
endif()
