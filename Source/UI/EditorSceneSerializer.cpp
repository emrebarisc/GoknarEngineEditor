#include "EditorSceneSerializer.h"

#include <string>

#include "Goknar/Application.h"
#include "Goknar/Components/Component.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Engine.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Helpers/SceneParser.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"

#include "tinyxml2.h"

#include "UI/EditorRuntimeDynamicObjectFactoryRegistrar.h"

namespace
{
	constexpr const char* kConstructorOwnedComponentsOmittedAttribute = "EditorReflectedComponentsOmitted";

	bool ShouldWriteObject(Scene* scene, ObjectBase* object)
	{
		if (!object || (scene && scene->GetIsObjectFromReferencedScene(object)))
		{
			return false;
		}

		if (object->GetName().find("__Editor__") != std::string::npos)
		{
			return false;
		}

		return !dynamic_cast<DebugObject*>(object);
	}

	void ApplyEditorComponentSerialization(ObjectBase* object, tinyxml2::XMLElement* componentsElement)
	{
		if (!object || !componentsElement)
		{
			return;
		}

		DynamicObjectFactory* factory = DynamicObjectFactory::GetInstance();
		const std::vector<Component*>& components = object->GetComponents();
		tinyxml2::XMLElement* componentElement = componentsElement->FirstChildElement();
		for (Component* component : components)
		{
			if (!componentElement)
			{
				break;
			}

			tinyxml2::XMLElement* nextComponentElement = componentElement->NextSiblingElement();
			if (EditorRuntimeDynamicObjectFactoryRegistrar::IsConstructorOwnedComponent(component))
			{
				componentsElement->DeleteChild(componentElement);
				componentElement = nextComponentElement;
				continue;
			}

			const std::string registeredClassName = factory->GetRegisteredComponentClassName(component);
			if (!registeredClassName.empty() && registeredClassName != componentElement->Name())
			{
				componentElement->SetAttribute("EditorClassName", registeredClassName.c_str());
			}

			componentElement = nextComponentElement;
		}
	}

	void ApplyRegisteredObjectClassNames(Scene* scene, ObjectBase* object, tinyxml2::XMLElement* objectElement)
	{
		if (!object || !objectElement)
		{
			return;
		}

		DynamicObjectFactory* factory = DynamicObjectFactory::GetInstance();
		const std::string registeredClassName = factory->GetRegisteredObjectClassName(object);
		if (!registeredClassName.empty())
		{
			objectElement->SetName(registeredClassName.c_str());
		}

		ApplyEditorComponentSerialization(object, objectElement->FirstChildElement("Components"));

		tinyxml2::XMLElement* childrenElement = objectElement->FirstChildElement("Children");
		tinyxml2::XMLElement* childObjectElement = childrenElement ? childrenElement->FirstChildElement() : nullptr;
		for (ObjectBase* childObject : object->GetChildren())
		{
			if (!ShouldWriteObject(scene, childObject))
			{
				continue;
			}

			if (!childObjectElement)
			{
				break;
			}

			ApplyRegisteredObjectClassNames(scene, childObject, childObjectElement);
			childObjectElement = childObjectElement->NextSiblingElement();
		}
	}

	void ApplyRegisteredObjectClassNames(Scene* scene, tinyxml2::XMLDocument& document)
	{
		tinyxml2::XMLElement* rootElement = document.RootElement();
		tinyxml2::XMLElement* objectsElement = rootElement ? rootElement->FirstChildElement("Objects") : nullptr;
		if (!scene || !objectsElement)
		{
			return;
		}

		tinyxml2::XMLElement* objectElement = objectsElement->FirstChildElement();
		for (ObjectBase* object : scene->GetObjects())
		{
			if (!ShouldWriteObject(scene, object) || object->GetParent())
			{
				continue;
			}

			if (!objectElement)
			{
				break;
			}

			ApplyRegisteredObjectClassNames(scene, object, objectElement);
			objectElement = objectElement->NextSiblingElement();
		}
	}
}

bool EditorSceneSerializer::OpenScene(const std::string& path)
{
	EditorRuntimeDynamicObjectFactoryRegistrar::ClearConstructorOwnedComponentMarkers();
	const bool didOpenScene = engine->GetApplication()->OpenScene(path);
	return didOpenScene;
}

void EditorSceneSerializer::SaveScene(Scene* scene, const std::string& filePath)
{
	SceneParser::SaveScene(scene, filePath);

	tinyxml2::XMLDocument document;
	if (document.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
	{
		return;
	}

	if (tinyxml2::XMLElement* rootElement = document.RootElement())
	{
		rootElement->SetAttribute(kConstructorOwnedComponentsOmittedAttribute, true);
	}

	ApplyRegisteredObjectClassNames(scene, document);
	document.SaveFile(filePath.c_str());
}
