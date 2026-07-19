function(goknar_get_editor_game_source_dirs OUT_SOURCE_DIRS)
    set(EDITOR_GAME_SOURCE_DIRS)

    if(EDITOR_ACTIVE_PROJECT_PATH AND EXISTS "${EDITOR_ACTIVE_PROJECT_PATH}/CMakeLists.txt")
        file(STRINGS "${EDITOR_ACTIVE_PROJECT_PATH}/CMakeLists.txt" PROJECT_CMAKE_LINES)

        set(IS_READING_SUBDIRS FALSE)
        foreach(PROJECT_CMAKE_LINE ${PROJECT_CMAKE_LINES})
            string(REGEX REPLACE "#.*$" "" PROJECT_CMAKE_LINE "${PROJECT_CMAKE_LINE}")
            string(STRIP "${PROJECT_CMAKE_LINE}" PROJECT_CMAKE_LINE)

            if(NOT IS_READING_SUBDIRS AND PROJECT_CMAKE_LINE MATCHES "^set\\([ \t]*SUBDIRS([ \t].*)?$")
                set(IS_READING_SUBDIRS TRUE)
                string(REGEX REPLACE "^set\\([ \t]*SUBDIRS" "" PROJECT_CMAKE_LINE "${PROJECT_CMAKE_LINE}")
                string(STRIP "${PROJECT_CMAKE_LINE}" PROJECT_CMAKE_LINE)
            endif()

            if(IS_READING_SUBDIRS)
                if(PROJECT_CMAKE_LINE MATCHES "\\)")
                    string(REGEX REPLACE "\\).*" "" PROJECT_CMAKE_LINE "${PROJECT_CMAKE_LINE}")
                    set(IS_READING_SUBDIRS FALSE)
                endif()

                string(STRIP "${PROJECT_CMAKE_LINE}" PROJECT_CMAKE_LINE)
                if(PROJECT_CMAKE_LINE STREQUAL "")
                    continue()
                endif()

                string(REPLACE "\${SOURCE_DIR_NAME}" "Source" PROJECT_CMAKE_LINE "${PROJECT_CMAKE_LINE}")
                string(REPLACE "\"" "" PROJECT_CMAKE_LINE "${PROJECT_CMAKE_LINE}")

                foreach(PROJECT_SUBDIR_TOKEN ${PROJECT_CMAKE_LINE})
                    string(STRIP "${PROJECT_SUBDIR_TOKEN}" PROJECT_SUBDIR_TOKEN)
                    if(PROJECT_SUBDIR_TOKEN MATCHES "^Source(/.*)?$")
                        get_filename_component(PROJECT_SUBDIR_PATH "${EDITOR_ACTIVE_PROJECT_PATH}/${PROJECT_SUBDIR_TOKEN}" ABSOLUTE)
                        list(APPEND EDITOR_GAME_SOURCE_DIRS "${PROJECT_SUBDIR_PATH}")
                    endif()
                endforeach()
            endif()
        endforeach()
    endif()

    if(NOT EDITOR_GAME_SOURCE_DIRS AND EDITOR_ACTIVE_PROJECT_SOURCE_ROOT)
        list(APPEND EDITOR_GAME_SOURCE_DIRS "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}")
    endif()

    list(REMOVE_DUPLICATES EDITOR_GAME_SOURCE_DIRS)
    set(${OUT_SOURCE_DIRS} "${EDITOR_GAME_SOURCE_DIRS}" PARENT_SCOPE)
endfunction()

function(goknar_is_path_in_editor_game_source_dirs PATH_VALUE OUT_RESULT)
    get_filename_component(PATH_PARENT_DIR "${PATH_VALUE}" DIRECTORY)
    file(TO_CMAKE_PATH "${PATH_PARENT_DIR}" NORMALIZED_PATH_PARENT_DIR)
    set(IS_IN_EDITOR_GAME_SOURCE_DIRS FALSE)

    foreach(EDITOR_GAME_SOURCE_DIR ${ARGN})
        file(TO_CMAKE_PATH "${EDITOR_GAME_SOURCE_DIR}" NORMALIZED_EDITOR_GAME_SOURCE_DIR)
        string(REGEX REPLACE "/$" "" NORMALIZED_EDITOR_GAME_SOURCE_DIR "${NORMALIZED_EDITOR_GAME_SOURCE_DIR}")
        if(NORMALIZED_PATH_PARENT_DIR STREQUAL NORMALIZED_EDITOR_GAME_SOURCE_DIR)
            set(IS_IN_EDITOR_GAME_SOURCE_DIRS TRUE)
            break()
        endif()
    endforeach()

    set(${OUT_RESULT} "${IS_IN_EDITOR_GAME_SOURCE_DIRS}" PARENT_SCOPE)
endfunction()

function(goknar_configure_editor_game_project_sources OUT_PROJECT_SOURCES OUT_REGISTRATION_SOURCE OUT_PROJECT_STAMP_FILE)
    set(EDITOR_GAME_PROJECT_SOURCES)

    set(EDITOR_GAME_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    file(MAKE_DIRECTORY "${EDITOR_GAME_GENERATED_DIR}")
    set(EDITOR_GAME_REGISTRATION_SOURCE "${EDITOR_GAME_GENERATED_DIR}/RegisterGameEditorClasses.cpp")
    set(EDITOR_GAME_PROJECT_STAMP_FILE "${EDITOR_GAME_GENERATED_DIR}/CompiledGameEditorProject.ini")
    set(EDITOR_GAME_COMPANION_GOKNAR_HEADER "${EDITOR_GAME_GENERATED_DIR}/Goknar.h")

    set(COMPILED_PROJECT_ROOT "")
    set(INCLUDE_LINES "")
    set(REGISTER_LINES "")

    if(EDITOR_ACTIVE_PROJECT_SOURCE_ROOT AND EXISTS "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}")
        file(TO_CMAKE_PATH "${EDITOR_ACTIVE_PROJECT_PATH}" COMPILED_PROJECT_ROOT)
        goknar_get_editor_game_source_dirs(EDITOR_GAME_SOURCE_DIRS)

        file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.h"
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.hpp"
        )
        list(SORT PROJECT_HEADERS)

        set(REGISTERED_CANDIDATE_CLASS_NAMES)
        set(INCLUDED_CANDIDATE_HEADERS)
        foreach(PROJECT_HEADER ${PROJECT_HEADERS})
            goknar_is_path_in_editor_game_source_dirs("${PROJECT_HEADER}" HEADER_IS_IN_SOURCE_DIRS ${EDITOR_GAME_SOURCE_DIRS})
            if(NOT HEADER_IS_IN_SOURCE_DIRS)
                continue()
            endif()

            file(RELATIVE_PATH PROJECT_HEADER_RELATIVE_PATH "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}" "${PROJECT_HEADER}")
            file(TO_CMAKE_PATH "${PROJECT_HEADER_RELATIVE_PATH}" PROJECT_HEADER_RELATIVE_PATH)

            file(READ "${PROJECT_HEADER}" PROJECT_HEADER_CONTENT)
            string(REGEX MATCHALL "(class|struct)[ \t\r\n]+(GOKNAR_API[ \t\r\n]+)?[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*(:[^{;]*)?[ \t\r\n]*\\{" CLASS_DECLARATIONS "${PROJECT_HEADER_CONTENT}")

            set(HEADER_HAS_CANDIDATE FALSE)
            foreach(CLASS_DECLARATION ${CLASS_DECLARATIONS})
                string(REGEX REPLACE "^.*(class|struct)[ \t\r\n]+" "" CLASS_TAIL "${CLASS_DECLARATION}")
                string(REGEX REPLACE "^GOKNAR_API[ \t\r\n]+" "" CLASS_TAIL "${CLASS_TAIL}")
                string(REGEX REPLACE "^[ \t\r\n]+" "" CLASS_TAIL "${CLASS_TAIL}")
                string(REGEX REPLACE "[ \t\r\n:\\{].*$" "" CLASS_NAME "${CLASS_TAIL}")

                if(NOT CLASS_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
                    continue()
                endif()

                list(FIND REGISTERED_CANDIDATE_CLASS_NAMES "${CLASS_NAME}" CLASS_NAME_INDEX)
                if(CLASS_NAME_INDEX EQUAL -1)
                    list(APPEND REGISTERED_CANDIDATE_CLASS_NAMES "${CLASS_NAME}")
                    string(APPEND REGISTER_LINES "\tRegisterGameEditorClass<${CLASS_NAME}>(\"${CLASS_NAME}\", \"${PROJECT_HEADER_RELATIVE_PATH}\");\n")
                    set(HEADER_HAS_CANDIDATE TRUE)
                else()
                    message(WARNING "Skipping duplicate game editor class candidate named ${CLASS_NAME} from ${PROJECT_HEADER}. DynamicObjectFactory registration uses simple class names.")
                endif()
            endforeach()

            if(HEADER_HAS_CANDIDATE)
                list(FIND INCLUDED_CANDIDATE_HEADERS "${PROJECT_HEADER}" HEADER_INDEX)
                if(HEADER_INDEX EQUAL -1)
                    string(APPEND INCLUDE_LINES "#include \"${PROJECT_HEADER_RELATIVE_PATH}\"\n")
                    list(APPEND INCLUDED_CANDIDATE_HEADERS "${PROJECT_HEADER}")
                endif()
            endif()
        endforeach()

        file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.c"
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cc"
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cpp"
            "${EDITOR_ACTIVE_PROJECT_SOURCE_ROOT}/*.cxx"
        )
        list(SORT PROJECT_SOURCES)

        foreach(PROJECT_SOURCE ${PROJECT_SOURCES})
            goknar_is_path_in_editor_game_source_dirs("${PROJECT_SOURCE}" SOURCE_IS_IN_SOURCE_DIRS ${EDITOR_GAME_SOURCE_DIRS})
            if(NOT SOURCE_IS_IN_SOURCE_DIRS)
                continue()
            endif()

            get_filename_component(PROJECT_SOURCE_NAME "${PROJECT_SOURCE}" NAME)
            set(SHOULD_INCLUDE_PROJECT_SOURCE TRUE)

            if(PROJECT_SOURCE_NAME STREQUAL "GPUPreference.cpp")
                set(SHOULD_INCLUDE_PROJECT_SOURCE FALSE)
            endif()

            if(SHOULD_INCLUDE_PROJECT_SOURCE)
                list(APPEND EDITOR_GAME_PROJECT_SOURCES "${PROJECT_SOURCE}")
            endif()
        endforeach()
    endif()

    file(WRITE "${EDITOR_GAME_PROJECT_STAMP_FILE}" "[Editor]\nCompiledProjectPath=${COMPILED_PROJECT_ROOT}\n")
    file(WRITE "${EDITOR_GAME_COMPANION_GOKNAR_HEADER}" "#pragma once\n\n#include \"Goknar/Core.h\"\n#include \"Goknar/Engine.h\"\n#include \"Goknar/Application.h\"\n#include \"Goknar/Log.h\"\n#include \"Goknar/Profiling/ProfileMacros.h\"\n")

    set(GENERATED_SOURCE "// Generated by CMake. Do not edit.\n")
    string(APPEND GENERATED_SOURCE "#include <type_traits>\n\n")
    string(APPEND GENERATED_SOURCE "#include \"UI/EditorGameClassRegistration.h\"\n")
    if(INCLUDE_LINES)
        string(APPEND GENERATED_SOURCE "\n${INCLUDE_LINES}")
    endif()
    string(APPEND GENERATED_SOURCE "\nnamespace\n{\n")
    string(APPEND GENERATED_SOURCE "\ttemplate <typename CandidateType>\n")
    string(APPEND GENERATED_SOURCE "\tvoid RegisterGameEditorClass(const char* className, const char* includePath)\n")
    string(APPEND GENERATED_SOURCE "\t{\n")
    string(APPEND GENERATED_SOURCE "\t\tif constexpr (std::is_base_of_v<ObjectBase, CandidateType> && !std::is_same_v<ObjectBase, CandidateType> && std::is_default_constructible_v<CandidateType>)\n")
    string(APPEND GENERATED_SOURCE "\t\t{\n")
    string(APPEND GENERATED_SOURCE "\t\t\tEditorGameClassRegistration::RegisterObjectClass<CandidateType>(className, includePath);\n")
    string(APPEND GENERATED_SOURCE "\t\t}\n")
    string(APPEND GENERATED_SOURCE "\t\telse if constexpr (std::is_base_of_v<Component, CandidateType> && !std::is_same_v<Component, CandidateType> && std::is_constructible_v<CandidateType, Component*>)\n")
    string(APPEND GENERATED_SOURCE "\t\t{\n")
    string(APPEND GENERATED_SOURCE "\t\t\tEditorGameClassRegistration::RegisterComponentClass<CandidateType>(className, includePath);\n")
    string(APPEND GENERATED_SOURCE "\t\t}\n")
    string(APPEND GENERATED_SOURCE "\t\telse\n")
    string(APPEND GENERATED_SOURCE "\t\t{\n")
    string(APPEND GENERATED_SOURCE "\t\t\t(void)className;\n")
    string(APPEND GENERATED_SOURCE "\t\t\t(void)includePath;\n")
    string(APPEND GENERATED_SOURCE "\t\t}\n")
    string(APPEND GENERATED_SOURCE "\t}\n")
    string(APPEND GENERATED_SOURCE "}\n\n")
    string(APPEND GENERATED_SOURCE "void RegisterGameEditorClasses()\n")
    string(APPEND GENERATED_SOURCE "{\n")
    if(REGISTER_LINES)
        string(APPEND GENERATED_SOURCE "${REGISTER_LINES}")
    endif()
    string(APPEND GENERATED_SOURCE "}\n")

    file(WRITE "${EDITOR_GAME_REGISTRATION_SOURCE}" "${GENERATED_SOURCE}")

    set(${OUT_PROJECT_SOURCES} "${EDITOR_GAME_PROJECT_SOURCES}" PARENT_SCOPE)
    set(${OUT_REGISTRATION_SOURCE} "${EDITOR_GAME_REGISTRATION_SOURCE}" PARENT_SCOPE)
    set(${OUT_PROJECT_STAMP_FILE} "${EDITOR_GAME_PROJECT_STAMP_FILE}" PARENT_SCOPE)
endfunction()
