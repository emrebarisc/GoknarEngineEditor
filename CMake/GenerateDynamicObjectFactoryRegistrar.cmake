cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED OUTPUT_HEADER OR OUTPUT_HEADER STREQUAL "")
	message(FATAL_ERROR "OUTPUT_HEADER is required")
endif()

if(NOT DEFINED OUTPUT_SOURCE OR OUTPUT_SOURCE STREQUAL "")
	message(FATAL_ERROR "OUTPUT_SOURCE is required")
endif()

set(KNOWN_OBJECT_BASES
	ObjectBase
	DebugObject
	HUD
	PhysicsObject
	OverlappingPhysicsObject
	RigidBody
	Character
	ReflectionProbeObject
	PlayerStart
)

set(KNOWN_COMPONENT_BASES
	Component
	RenderComponent
	SocketComponent
	CameraComponent
	NavigationTreeComponent
	PointLightComponent
	MeshComponent
	DynamicMeshComponent
	InstancedStaticMeshComponent
	StaticMeshComponent
	SkeletalMeshComponent
	ParticleSystemComponent
	BillboardParticleSystemComponent
	StaticMeshParticleSystemComponent
	CollisionComponent
	BoxCollisionComponent
	CapsuleCollisionComponent
	SphereCollisionComponent
	MovingTriangleMeshCollisionComponent
	NonMovingTriangleMeshCollisionComponent
	MultipleCollisionComponent
	PhysicsMovementComponent
)

set(ABSTRACT_COMPONENT_PLACEHOLDER_BASES
	RenderComponent
	MeshComponent
	ParticleSystemComponent
)

set(PHYSICS_COMPONENT_BASES
	CollisionComponent
	BoxCollisionComponent
	CapsuleCollisionComponent
	SphereCollisionComponent
	MovingTriangleMeshCollisionComponent
	NonMovingTriangleMeshCollisionComponent
	MultipleCollisionComponent
)

set(OVERLAPPING_COMPONENT_BASES
	PhysicsMovementComponent
)

set(IGNORED_CLASS_DECLARATION_TOKENS
	GOKNAR_API
	final
)

set(IGNORED_BASE_DECLARATION_TOKENS
	public
	protected
	private
	virtual
)

function(make_identifier INPUT_VALUE OUTPUT_VARIABLE)
	string(REGEX REPLACE "[^A-Za-z0-9_]" "_" IDENTIFIER "${INPUT_VALUE}")
	set(${OUTPUT_VARIABLE} "${IDENTIFIER}" PARENT_SCOPE)
endfunction()

function(is_header_file FILE_PATH OUTPUT_VARIABLE)
	get_filename_component(FILE_EXTENSION "${FILE_PATH}" EXT)
	string(TOLOWER "${FILE_EXTENSION}" FILE_EXTENSION)
	if(FILE_EXTENSION STREQUAL ".h" OR
		FILE_EXTENSION STREQUAL ".hh" OR
		FILE_EXTENSION STREQUAL ".hpp" OR
		FILE_EXTENSION STREQUAL ".hxx")
		set(${OUTPUT_VARIABLE} TRUE PARENT_SCOPE)
	else()
		set(${OUTPUT_VARIABLE} FALSE PARENT_SCOPE)
	endif()
endfunction()

function(get_relative_include FILE_PATH OUTPUT_VARIABLE)
	set(RELATIVE_INCLUDE "")

	if(DEFINED INCLUDE_ROOT AND NOT INCLUDE_ROOT STREQUAL "" AND EXISTS "${INCLUDE_ROOT}")
		file(RELATIVE_PATH CANDIDATE_INCLUDE "${INCLUDE_ROOT}" "${FILE_PATH}")
		if(NOT CANDIDATE_INCLUDE MATCHES "^\\.\\.")
			set(RELATIVE_INCLUDE "${CANDIDATE_INCLUDE}")
		endif()
	endif()

	if(RELATIVE_INCLUDE STREQUAL "" AND DEFINED SCAN_ROOT AND NOT SCAN_ROOT STREQUAL "" AND EXISTS "${SCAN_ROOT}")
		file(RELATIVE_PATH CANDIDATE_INCLUDE "${SCAN_ROOT}" "${FILE_PATH}")
		if(NOT CANDIDATE_INCLUDE MATCHES "^\\.\\.")
			set(RELATIVE_INCLUDE "${CANDIDATE_INCLUDE}")
		endif()
	endif()

	string(REPLACE "\\" "/" RELATIVE_INCLUDE "${RELATIVE_INCLUDE}")
	set(${OUTPUT_VARIABLE} "${RELATIVE_INCLUDE}" PARENT_SCOPE)
endfunction()

function(class_derives_from CLASS_NAME BASE_CLASS_LIST OUTPUT_VARIABLE)
	set(IS_DERIVED FALSE)

	if("${CLASS_NAME}" IN_LIST BASE_CLASS_LIST)
		set(IS_DERIVED TRUE)
	elseif("${CLASS_NAME}" IN_LIST CLASS_NAMES)
		make_identifier("${CLASS_NAME}" CLASS_VARIABLE_NAME)
		foreach(BASE_CLASS IN LISTS CLASS_BASES_${CLASS_VARIABLE_NAME})
			if("${BASE_CLASS}" IN_LIST BASE_CLASS_LIST)
				set(IS_DERIVED TRUE)
				break()
			endif()

			if("${BASE_CLASS}" IN_LIST CLASS_NAMES)
				class_derives_from("${BASE_CLASS}" "${BASE_CLASS_LIST}" BASE_IS_DERIVED)
				if(BASE_IS_DERIVED)
					set(IS_DERIVED TRUE)
					break()
				endif()
			endif()
		endforeach()
	endif()

	set(${OUTPUT_VARIABLE} "${IS_DERIVED}" PARENT_SCOPE)
endfunction()

function(get_placeholder_base CLASS_NAME KNOWN_BASE_LIST FALLBACK_BASE OUTPUT_VARIABLE)
	set(PLACEHOLDER_BASE "")

	if("${CLASS_NAME}" IN_LIST CLASS_NAMES)
		make_identifier("${CLASS_NAME}" CLASS_VARIABLE_NAME)
		foreach(BASE_CLASS IN LISTS CLASS_BASES_${CLASS_VARIABLE_NAME})
			if("${BASE_CLASS}" IN_LIST KNOWN_BASE_LIST)
				set(PLACEHOLDER_BASE "${BASE_CLASS}")
				break()
			endif()
		endforeach()

		if(PLACEHOLDER_BASE STREQUAL "")
			foreach(BASE_CLASS IN LISTS CLASS_BASES_${CLASS_VARIABLE_NAME})
				if("${BASE_CLASS}" IN_LIST CLASS_NAMES)
					get_placeholder_base("${BASE_CLASS}" "${KNOWN_BASE_LIST}" "${FALLBACK_BASE}" BASE_PLACEHOLDER)
					if(NOT BASE_PLACEHOLDER STREQUAL "${FALLBACK_BASE}")
						set(PLACEHOLDER_BASE "${BASE_PLACEHOLDER}")
						break()
					endif()
				endif()
			endforeach()
		endif()
	endif()

	if(PLACEHOLDER_BASE STREQUAL "" OR "${PLACEHOLDER_BASE}" IN_LIST ABSTRACT_COMPONENT_PLACEHOLDER_BASES)
		set(PLACEHOLDER_BASE "${FALLBACK_BASE}")
	endif()

	set(${OUTPUT_VARIABLE} "${PLACEHOLDER_BASE}" PARENT_SCOPE)
endfunction()

function(get_class_depth CLASS_NAME OUTPUT_VARIABLE)
	set(MAX_DEPTH 0)

	if("${CLASS_NAME}" IN_LIST CLASS_NAMES)
		make_identifier("${CLASS_NAME}" CLASS_VARIABLE_NAME)
		foreach(BASE_CLASS IN LISTS CLASS_BASES_${CLASS_VARIABLE_NAME})
			if("${BASE_CLASS}" IN_LIST CLASS_NAMES)
				get_class_depth("${BASE_CLASS}" BASE_DEPTH)
				math(EXPR CANDIDATE_DEPTH "${BASE_DEPTH} + 1")
				if(CANDIDATE_DEPTH GREATER MAX_DEPTH)
					set(MAX_DEPTH "${CANDIDATE_DEPTH}")
				endif()
			elseif("${BASE_CLASS}" IN_LIST KNOWN_OBJECT_BASES OR "${BASE_CLASS}" IN_LIST KNOWN_COMPONENT_BASES)
				if(1 GREATER MAX_DEPTH)
					set(MAX_DEPTH 1)
				endif()
			endif()
		endforeach()
	endif()

	set(${OUTPUT_VARIABLE} "${MAX_DEPTH}" PARENT_SCOPE)
endfunction()

function(make_depth_sort_key CLASS_NAME OUTPUT_VARIABLE)
	get_class_depth("${CLASS_NAME}" CLASS_DEPTH)
	set(DEPTH_KEY "000${CLASS_DEPTH}")
	string(LENGTH "${DEPTH_KEY}" DEPTH_KEY_LENGTH)
	math(EXPR DEPTH_KEY_START "${DEPTH_KEY_LENGTH} - 4")
	string(SUBSTRING "${DEPTH_KEY}" "${DEPTH_KEY_START}" 4 DEPTH_KEY)
	set(${OUTPUT_VARIABLE} "${DEPTH_KEY}|${CLASS_NAME}" PARENT_SCOPE)
endfunction()

function(write_if_different FILE_PATH FILE_CONTENT)
	set(EXISTING_CONTENT "")
	if(EXISTS "${FILE_PATH}")
		file(READ "${FILE_PATH}" EXISTING_CONTENT)
	endif()

	if(NOT EXISTING_CONTENT STREQUAL FILE_CONTENT)
		file(WRITE "${FILE_PATH}" "${FILE_CONTENT}")
	endif()
endfunction()

set(SCAN_FILES)
if(DEFINED SCAN_ROOT AND NOT SCAN_ROOT STREQUAL "" AND EXISTS "${SCAN_ROOT}")
	file(GLOB_RECURSE SCAN_FILES LIST_DIRECTORIES false
		"${SCAN_ROOT}/*.h"
		"${SCAN_ROOT}/*.hh"
		"${SCAN_ROOT}/*.hpp"
		"${SCAN_ROOT}/*.hxx"
		"${SCAN_ROOT}/*.cpp"
		"${SCAN_ROOT}/*.cc"
		"${SCAN_ROOT}/*.cxx"
	)
endif()

set(HEADER_FILES)
set(SOURCE_FILES)
foreach(SCAN_FILE IN LISTS SCAN_FILES)
	is_header_file("${SCAN_FILE}" IS_HEADER)
	if(IS_HEADER)
		list(APPEND HEADER_FILES "${SCAN_FILE}")
	else()
		list(APPEND SOURCE_FILES "${SCAN_FILE}")
	endif()
endforeach()

list(SORT HEADER_FILES)
list(SORT SOURCE_FILES)
set(SCAN_FILES ${HEADER_FILES} ${SOURCE_FILES})

set(CLASS_NAMES)

foreach(SCAN_FILE IN LISTS SCAN_FILES)
	file(READ "${SCAN_FILE}" SOURCE_CONTENT)
	string(REGEX REPLACE "//[^\r\n]*" " " SOURCE_CONTENT "${SOURCE_CONTENT}")
	string(REGEX MATCHALL "(class|struct)[ \t\r\n]+[^:;{}]+:[^{;]+\\{" CLASS_MATCHES "${SOURCE_CONTENT}")

	foreach(CLASS_MATCH IN LISTS CLASS_MATCHES)
		string(REGEX REPLACE "^(class|struct)[ \t\r\n]+([^:]+):.*$" "\\2" CLASS_DECLARATION "${CLASS_MATCH}")
		string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" CLASS_DECLARATION_TOKENS "${CLASS_DECLARATION}")

		set(CLASS_NAME "")
		foreach(CLASS_TOKEN IN LISTS CLASS_DECLARATION_TOKENS)
			if(NOT "${CLASS_TOKEN}" IN_LIST IGNORED_CLASS_DECLARATION_TOKENS)
				set(CLASS_NAME "${CLASS_TOKEN}")
			endif()
		endforeach()

		if(CLASS_NAME STREQUAL "")
			continue()
		endif()

		string(REGEX REPLACE "^(class|struct)[ \t\r\n]+[^:]+:[ \t\r\n]*([^\\{]+)\\{.*$" "\\2" BASE_DECLARATION "${CLASS_MATCH}")
		string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_:]*" BASE_DECLARATION_TOKENS "${BASE_DECLARATION}")

		set(BASE_CLASSES)
		foreach(BASE_TOKEN IN LISTS BASE_DECLARATION_TOKENS)
			if("${BASE_TOKEN}" IN_LIST IGNORED_BASE_DECLARATION_TOKENS)
				continue()
			endif()

			string(REGEX REPLACE "^.*::" "" BASE_CLASS "${BASE_TOKEN}")
			list(APPEND BASE_CLASSES "${BASE_CLASS}")
		endforeach()

		if(NOT "${CLASS_NAME}" IN_LIST CLASS_NAMES)
			list(APPEND CLASS_NAMES "${CLASS_NAME}")
		endif()

		make_identifier("${CLASS_NAME}" CLASS_VARIABLE_NAME)
		set(CLASS_BASES_${CLASS_VARIABLE_NAME} "${BASE_CLASSES}")
	endforeach()
endforeach()

set(OBJECT_REGISTRATION_ENTRIES)
set(COMPONENT_REGISTRATION_ENTRIES)

foreach(CLASS_NAME IN LISTS CLASS_NAMES)
	make_identifier("${CLASS_NAME}" CLASS_VARIABLE_NAME)
	class_derives_from("${CLASS_NAME}" "${KNOWN_OBJECT_BASES}" IS_OBJECT_CLASS)
	class_derives_from("${CLASS_NAME}" "${KNOWN_COMPONENT_BASES}" IS_COMPONENT_CLASS)

	if(IS_OBJECT_CLASS)
		make_depth_sort_key("${CLASS_NAME}" SORT_KEY)
		list(APPEND OBJECT_REGISTRATION_ENTRIES "${SORT_KEY}")
	elseif(IS_COMPONENT_CLASS)
		make_depth_sort_key("${CLASS_NAME}" SORT_KEY)
		list(APPEND COMPONENT_REGISTRATION_ENTRIES "${SORT_KEY}")
	endif()
endforeach()

list(SORT OBJECT_REGISTRATION_ENTRIES)
list(SORT COMPONENT_REGISTRATION_ENTRIES)

set(HEADER_CONTENT "#pragma once\n\nnamespace EditorDynamicObjectFactoryRegistrar\n{\n\tvoid Register();\n}\n")

set(SOURCE_CONTENT "#include \"Generated/EditorDynamicObjectFactoryRegistrar.generated.h\"\n\n#include <string>\n\n#include \"Goknar/Factories/DynamicObjectFactory.h\"\n#include \"Goknar/ObjectBase.h\"\n#include \"Goknar/Debug/DebugDrawer.h\"\n#include \"Goknar/Objects/PlayerStart.h\"\n#include \"Goknar/Objects/ReflectionProbeObject.h\"\n#include \"Goknar/Physics/Character.h\"\n#include \"Goknar/Physics/OverlappingPhysicsObject.h\"\n#include \"Goknar/Physics/PhysicsObject.h\"\n#include \"Goknar/Physics/RigidBody.h\"\n#include \"Goknar/UI/HUD.h\"\n#include \"Goknar/Components/CameraComponent.h\"\n#include \"Goknar/Components/Component.h\"\n#include \"Goknar/Components/DynamicMeshComponent.h\"\n#include \"Goknar/Components/InstancedStaticMeshComponent.h\"\n#include \"Goknar/Components/LightComponents/PointLightComponent.h\"\n#include \"Goknar/Components/ParticleSystemComponent.h\"\n#include \"Goknar/Components/RenderComponent.h\"\n#include \"Goknar/Components/SkeletalMeshComponent.h\"\n#include \"Goknar/Components/SocketComponent.h\"\n#include \"Goknar/Components/StaticMeshComponent.h\"\n#include \"Goknar/Navigation/NavigationTreeComponent.h\"\n#include \"Goknar/Physics/Components/BoxCollisionComponent.h\"\n#include \"Goknar/Physics/Components/CapsuleCollisionComponent.h\"\n#include \"Goknar/Physics/Components/CollisionComponent.h\"\n#include \"Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h\"\n#include \"Goknar/Physics/Components/MultipleCollisionComponent.h\"\n#include \"Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h\"\n#include \"Goknar/Physics/Components/PhysicsMovementComponent.h\"\n#include \"Goknar/Physics/Components/SphereCollisionComponent.h\"\n\nnamespace\n{\n\ttemplate <typename BaseType>\n\tclass EditorDynamicObjectPlaceholder : public BaseType\n\t{\n\tpublic:\n\t\texplicit EditorDynamicObjectPlaceholder(const char* registeredClassName) : BaseType(), registeredClassName_(registeredClassName)\n\t\t{\n\t\t\tthis->SetName(registeredClassName_);\n\t\t}\n\n\t\tconst std::string& GetRegisteredClassName() const\n\t\t{\n\t\t\treturn registeredClassName_;\n\t\t}\n\n\tprivate:\n\t\tstd::string registeredClassName_;\n\t};\n\n\ttemplate <typename BaseType>\n\tclass EditorDynamicComponentPlaceholder : public BaseType\n\t{\n\tpublic:\n\t\tEditorDynamicComponentPlaceholder(Component* parentComponent, const char* registeredClassName) : BaseType(parentComponent), registeredClassName_(registeredClassName)\n\t\t{\n\t\t}\n\n\t\tconst std::string& GetRegisteredClassName() const\n\t\t{\n\t\t\treturn registeredClassName_;\n\t\t}\n\n\tprivate:\n\t\tstd::string registeredClassName_;\n\t};\n}\n\nnamespace EditorDynamicObjectFactoryRegistrar\n{\n\tvoid Register()\n\t{\n\t\tstatic bool areProjectClassesRegistered = false;\n\t\tif (areProjectClassesRegistered)\n\t\t{\n\t\t\treturn;\n\t\t}\n\n\t\tDynamicObjectFactory* factory = DynamicObjectFactory::GetInstance();\n")

foreach(REGISTRATION_ENTRY IN LISTS OBJECT_REGISTRATION_ENTRIES)
	string(REGEX REPLACE "^[^|]+\\|" "" CLASS_NAME "${REGISTRATION_ENTRY}")
	get_placeholder_base("${CLASS_NAME}" "${KNOWN_OBJECT_BASES}" "ObjectBase" PLACEHOLDER_BASE)
	string(APPEND SOURCE_CONTENT "\t\tfactory->RegisterClass(\n\t\t\t\"${CLASS_NAME}\",\n\t\t\t[]() -> ObjectBase*\n\t\t\t{\n\t\t\t\treturn new EditorDynamicObjectPlaceholder<${PLACEHOLDER_BASE}>(\"${CLASS_NAME}\");\n\t\t\t},\n\t\t\t[](const ObjectBase* object) -> bool\n\t\t\t{\n\t\t\t\tconst auto* placeholder = dynamic_cast<const EditorDynamicObjectPlaceholder<${PLACEHOLDER_BASE}>*>(object);\n\t\t\t\treturn placeholder && placeholder->GetRegisteredClassName() == \"${CLASS_NAME}\";\n\t\t\t});\n")
endforeach()

if(OBJECT_REGISTRATION_ENTRIES AND COMPONENT_REGISTRATION_ENTRIES)
	string(APPEND SOURCE_CONTENT "\n")
endif()

foreach(REGISTRATION_ENTRY IN LISTS COMPONENT_REGISTRATION_ENTRIES)
	string(REGEX REPLACE "^[^|]+\\|" "" CLASS_NAME "${REGISTRATION_ENTRY}")
	class_derives_from("${CLASS_NAME}" "${OVERLAPPING_COMPONENT_BASES}" IS_OVERLAPPING_COMPONENT)
	class_derives_from("${CLASS_NAME}" "${PHYSICS_COMPONENT_BASES}" IS_PHYSICS_COMPONENT)

	if(IS_OVERLAPPING_COMPONENT)
		set(OWNER_REQUIREMENT "DynamicComponentOwnerRequirement::OverlappingPhysicsObject")
	elseif(IS_PHYSICS_COMPONENT)
		set(OWNER_REQUIREMENT "DynamicComponentOwnerRequirement::PhysicsObject")
	else()
		set(OWNER_REQUIREMENT "DynamicComponentOwnerRequirement::ObjectBase")
	endif()

	get_placeholder_base("${CLASS_NAME}" "${KNOWN_COMPONENT_BASES}" "Component" PLACEHOLDER_BASE)
	string(APPEND SOURCE_CONTENT "\t\tfactory->RegisterComponentClass(\n\t\t\t\"${CLASS_NAME}\",\n\t\t\t[](Component* parentComponent) -> Component*\n\t\t\t{\n\t\t\t\treturn new EditorDynamicComponentPlaceholder<${PLACEHOLDER_BASE}>(parentComponent, \"${CLASS_NAME}\");\n\t\t\t},\n\t\t\t[](const Component* component) -> bool\n\t\t\t{\n\t\t\t\tconst auto* placeholder = dynamic_cast<const EditorDynamicComponentPlaceholder<${PLACEHOLDER_BASE}>*>(component);\n\t\t\t\treturn placeholder && placeholder->GetRegisteredClassName() == \"${CLASS_NAME}\";\n\t\t\t},\n\t\t\t${OWNER_REQUIREMENT});\n")
endforeach()

string(APPEND SOURCE_CONTENT "\n\t\tareProjectClassesRegistered = true;\n\t}\n}\n\nnamespace\n{\n\tstruct EditorDynamicObjectFactoryAutoRegistrar\n\t{\n\t\tEditorDynamicObjectFactoryAutoRegistrar()\n\t\t{\n\t\t\tEditorDynamicObjectFactoryRegistrar::Register();\n\t\t}\n\t};\n\n\tEditorDynamicObjectFactoryAutoRegistrar editorDynamicObjectFactoryAutoRegistrar;\n}\n")

get_filename_component(OUTPUT_DIRECTORY "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
get_filename_component(OUTPUT_DIRECTORY "${OUTPUT_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")

write_if_different("${OUTPUT_HEADER}" "${HEADER_CONTENT}")
write_if_different("${OUTPUT_SOURCE}" "${SOURCE_CONTENT}")
