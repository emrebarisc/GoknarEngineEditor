#pragma once

#include <string>

class Component;
class ObjectBase;

namespace EditorRuntimeDynamicObjectFactoryRegistrar
{
	void RegisterProjectClasses(const std::string& projectRootPath);
	void SetApplyReflectionsOnCreate(bool shouldApplyReflectionsOnCreate);
	bool GetApplyReflectionsOnCreate();
	void ClearReflectedComponentMarkers();
	void ApplyReflectionsToObject(ObjectBase* object);
	bool IsReflectedComponent(const Component* component);
}
