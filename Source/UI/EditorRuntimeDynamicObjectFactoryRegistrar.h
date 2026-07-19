#pragma once

#include <string>

class Component;
class ObjectBase;

namespace EditorRuntimeDynamicObjectFactoryRegistrar
{
	void RegisterProjectClasses(const std::string& projectRootPath);
	void MarkConstructorOwnedComponents(ObjectBase* object);
	void ClearConstructorOwnedComponentMarkers();
	bool IsConstructorOwnedComponent(const Component* component);
}
