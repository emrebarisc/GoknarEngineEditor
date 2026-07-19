#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "Goknar/Components/Component.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Log.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Physics/Components/CollisionComponent.h"
#include "Goknar/Physics/Components/PhysicsMovementComponent.h"
#include "Goknar/Physics/OverlappingPhysicsObject.h"
#include "Goknar/Physics/PhysicsObject.h"

#include "UI/EditorRuntimeDynamicObjectFactoryRegistrar.h"

namespace EditorGameClassRegistration
{
	struct ClassMetadata
	{
		std::string className;
		std::string includePath;
		bool isComponent{ false };
	};

	void RegisterClassMetadata(const ClassMetadata& classMetadata);
	const std::vector<ClassMetadata>& GetRegisteredClassMetadata();

	inline bool IsDynamicClassNameAvailable(const std::string& className)
	{
		const auto& objectMap = DynamicObjectFactory::GetInstance()->GetObjectMap();
		const auto& componentMap = DynamicObjectFactory::GetInstance()->GetComponentMap();
		return objectMap.find(className) == objectMap.end() && componentMap.find(className) == componentMap.end();
	}

	template <typename ComponentType>
	constexpr DynamicComponentOwnerRequirement GetOwnerRequirement()
	{
		if constexpr (std::is_base_of_v<PhysicsMovementComponent, ComponentType>)
		{
			return DynamicComponentOwnerRequirement::OverlappingPhysicsObject;
		}
		else if constexpr (std::is_base_of_v<CollisionComponent, ComponentType>)
		{
			return DynamicComponentOwnerRequirement::PhysicsObject;
		}
		else
		{
			return DynamicComponentOwnerRequirement::ObjectBase;
		}
	}

	template <typename ObjectType>
	bool RegisterObjectClass(const std::string& className, const std::string& includePath = "")
	{
		if (!IsDynamicClassNameAvailable(className))
		{
			GOKNAR_CORE_WARN("Skipping game editor object class registration for %s because that dynamic class name is already registered.", className.c_str());
			return false;
		}

		DynamicObjectFactory::GetInstance()->RegisterClass(
			className,
			[className]() -> ObjectBase*
			{
				ObjectBase* object = new ObjectType();
				EditorRuntimeDynamicObjectFactoryRegistrar::MarkConstructorOwnedComponents(object);
				return object;
			},
			[](const ObjectBase* object) -> bool
			{
				return dynamic_cast<const ObjectType*>(object) != nullptr;
			});

		RegisterClassMetadata({ className, includePath, false });
		return true;
	}

	template <typename ComponentType>
	bool RegisterComponentClass(const std::string& className, const std::string& includePath = "")
	{
		if (!IsDynamicClassNameAvailable(className))
		{
			GOKNAR_CORE_WARN("Skipping game editor component class registration for %s because that dynamic class name is already registered.", className.c_str());
			return false;
		}

		DynamicObjectFactory::GetInstance()->RegisterComponentClass(
			className,
			[](Component* parentComponent) -> Component*
			{
				return new ComponentType(parentComponent);
			},
			[](const Component* component) -> bool
			{
				return dynamic_cast<const ComponentType*>(component) != nullptr;
			},
			GetOwnerRequirement<ComponentType>());

		RegisterClassMetadata({ className, includePath, true });
		return true;
	}
}
