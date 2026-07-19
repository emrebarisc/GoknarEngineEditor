#include "EditorRuntimeDynamicObjectFactoryRegistrar.h"

#include <unordered_set>

#include "Goknar/Components/Component.h"
#include "Goknar/Log.h"
#include "Goknar/ObjectBase.h"

#include "UI/EditorGameProjectBuildUtils.h"

void RegisterGameEditorClasses();

namespace
{
	std::unordered_set<const Component*> constructorOwnedComponents;

	bool CanRegisterCompiledGameEditorClasses(const std::string& projectRootPath)
	{
		if (EditorGameProjectBuildUtils::IsCompiledGameEditorProjectCurrent(projectRootPath))
		{
			return true;
		}

		const std::string compiledProjectRoot = EditorGameProjectBuildUtils::GetCompiledGameEditorProjectRoot();
		const std::string runtimeProjectRoot = EditorGameProjectBuildUtils::GetCurrentEditorConfigProjectRoot();
		GOKNAR_CORE_WARN(
			"Skipping compiled game editor class registration because the editor was built for %s but the current project is %s. Rebuild the editor to load game classes for this project.",
			compiledProjectRoot.c_str(),
			runtimeProjectRoot.c_str());
		return false;
	}

	void MarkConstructorOwnedComponentSet(ObjectBase* object)
	{
		if (!object)
		{
			return;
		}

		for (Component* component : object->GetComponents())
		{
			if (component)
			{
				constructorOwnedComponents.insert(component);
			}
		}
	}
}

void EditorRuntimeDynamicObjectFactoryRegistrar::RegisterProjectClasses(const std::string& projectRootPath)
{
	if (CanRegisterCompiledGameEditorClasses(projectRootPath))
	{
		RegisterGameEditorClasses();
	}
}

void EditorRuntimeDynamicObjectFactoryRegistrar::MarkConstructorOwnedComponents(ObjectBase* object)
{
	MarkConstructorOwnedComponentSet(object);
}

void EditorRuntimeDynamicObjectFactoryRegistrar::ClearConstructorOwnedComponentMarkers()
{
	constructorOwnedComponents.clear();
}

bool EditorRuntimeDynamicObjectFactoryRegistrar::IsConstructorOwnedComponent(const Component* component)
{
	return component && constructorOwnedComponents.find(component) != constructorOwnedComponents.end();
}
