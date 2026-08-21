#include "EditorSceneSerializer.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Goknar/Application.h"
#include "Goknar/Components/Component.h"
#include "Goknar/Components/LightComponents/DirectionalLightComponent.h"
#include "Goknar/Components/LightComponents/PointLightComponent.h"
#include "Goknar/Components/LightComponents/SpotLightComponent.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Engine.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Helpers/SceneParser.h"
#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"
#include "Goknar/Math/GoknarMath.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"

#include "tinyxml2.h"

#include "UI/EditorRuntimeDynamicObjectFactoryRegistrar.h"

namespace
{
	constexpr const char* kConstructorOwnedComponentsOmittedAttribute = "EditorReflectedComponentsOmitted";
	std::unordered_map<const DirectionalLight*, Vector3> authoredDirectionalLightDirections;
	std::unordered_map<const SpotLight*, Vector3> authoredSpotLightDirections;

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

	bool ShouldWriteLight(Light* light, bool isFromReferencedScene)
	{
		if (!light || isFromReferencedScene)
		{
			return false;
		}

		return light->GetName().find("__Editor__") == std::string::npos;
	}

	std::string SerializeVector3(const Vector3& vector)
	{
		return std::to_string(vector.x) + " " + std::to_string(vector.y) + " " + std::to_string(vector.z);
	}

	bool ReadVector3Element(const tinyxml2::XMLElement* element, Vector3& outValue)
	{
		if (!element || !element->GetText())
		{
			return false;
		}

		std::istringstream stream(element->GetText());
		Vector3 value;
		if (!(stream >> value.x >> value.y >> value.z))
		{
			return false;
		}

		outValue = value;
		return true;
	}

	void WriteVectorElement(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parentElement, const char* elementName, const Vector3& value)
	{
		if (!parentElement)
		{
			return;
		}

		tinyxml2::XMLElement* element = document.NewElement(elementName);
		element->SetText(SerializeVector3(value).c_str());
		parentElement->InsertEndChild(element);
	}

	void WriteFloatElement(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parentElement, const char* elementName, float value)
	{
		if (!parentElement)
		{
			return;
		}

		tinyxml2::XMLElement* element = document.NewElement(elementName);
		element->SetText(value);
		parentElement->InsertEndChild(element);
	}

	void WriteTextElement(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parentElement, const char* elementName, const std::string& value)
	{
		if (!parentElement)
		{
			return;
		}

		tinyxml2::XMLElement* element = document.NewElement(elementName);
		element->SetText(value.c_str());
		parentElement->InsertEndChild(element);
	}

	void WriteLightComponentCommonElements(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* componentElement, Light* light)
	{
		if (!light || !componentElement)
		{
			return;
		}

		WriteVectorElement(document, componentElement, "Color", light->GetColor());
		WriteFloatElement(document, componentElement, "Intensity", light->GetIntensity());
		WriteTextElement(document, componentElement, "IsCastingShadow", light->GetIsShadowEnabled() ? "1" : "0");
		WriteFloatElement(document, componentElement, "ShadowIntensity", light->GetShadowIntensity());
		WriteTextElement(
			document,
			componentElement,
			"ShadowMapResolution",
			std::to_string(light->GetShadowWidth()) + " " + std::to_string(light->GetShadowHeight()));
	}

	bool ApplyLightComponentSerialization(Component* component, tinyxml2::XMLDocument& document, tinyxml2::XMLElement* componentElement)
	{
		if (!component || !componentElement)
		{
			return false;
		}

		if (DirectionalLightComponent* directionalLightComponent = dynamic_cast<DirectionalLightComponent*>(component))
		{
			componentElement->SetName("DirectionalLightComponent");
			WriteLightComponentCommonElements(document, componentElement, directionalLightComponent->GetLight());
			return true;
		}

		if (PointLightComponent* pointLightComponent = dynamic_cast<PointLightComponent*>(component))
		{
			componentElement->SetName("PointLightComponent");
			PointLight* light = pointLightComponent->GetLight();
			WriteLightComponentCommonElements(document, componentElement, light);
			if (light)
			{
				WriteFloatElement(document, componentElement, "Radius", light->GetRadius());
			}
			return true;
		}

		if (SpotLightComponent* spotLightComponent = dynamic_cast<SpotLightComponent*>(component))
		{
			componentElement->SetName("SpotLightComponent");
			SpotLight* light = spotLightComponent->GetLight();
			WriteLightComponentCommonElements(document, componentElement, light);
			if (light)
			{
				WriteFloatElement(document, componentElement, "FalloffAngle", RADIAN_TO_DEGREE(light->GetFalloffAngle()));
				WriteFloatElement(document, componentElement, "CoverageAngle", RADIAN_TO_DEGREE(light->GetCoverageAngle()));
				WriteFloatElement(document, componentElement, "Radius", light->GetRadius());
			}
			return true;
		}

		return false;
	}

	template <typename ComponentType, typename LightType>
	void CollectComponentOwnedLights(ObjectBase* object, std::unordered_set<const LightType*>& lights)
	{
		if (!object)
		{
			return;
		}

		for (Component* component : object->GetComponents())
		{
			if (ComponentType* lightComponent = dynamic_cast<ComponentType*>(component))
			{
				if (LightType* light = lightComponent->GetLight())
				{
					lights.insert(light);
				}
			}
		}

		for (ObjectBase* childObject : object->GetChildren())
		{
			CollectComponentOwnedLights<ComponentType, LightType>(childObject, lights);
		}
	}

	template <typename ComponentType, typename LightType>
	std::unordered_set<const LightType*> CollectComponentOwnedLights(Scene* scene)
	{
		std::unordered_set<const LightType*> lights;
		if (!scene)
		{
			return lights;
		}

		for (ObjectBase* object : scene->GetObjects())
		{
			CollectComponentOwnedLights<ComponentType, LightType>(object, lights);
		}

		return lights;
	}

	template <typename LightType, typename IsReferencedSceneLight>
	void RemoveComponentOwnedLightElements(
		tinyxml2::XMLElement* lightsElement,
		const char* elementName,
		const std::vector<LightType*>& sceneLights,
		const std::unordered_set<const LightType*>& componentOwnedLights,
		IsReferencedSceneLight isReferencedSceneLight)
	{
		if (!lightsElement || componentOwnedLights.empty())
		{
			return;
		}

		tinyxml2::XMLElement* lightElement = lightsElement->FirstChildElement(elementName);
		for (LightType* light : sceneLights)
		{
			const bool isFromReferencedScene = light ? isReferencedSceneLight(light) : false;
			if (!ShouldWriteLight(light, isFromReferencedScene))
			{
				continue;
			}

			if (!lightElement)
			{
				break;
			}

			tinyxml2::XMLElement* nextLightElement = lightElement->NextSiblingElement(elementName);
			if (componentOwnedLights.find(light) != componentOwnedLights.end())
			{
				lightsElement->DeleteChild(lightElement);
			}

			lightElement = nextLightElement;
		}
	}

	void RemoveComponentOwnedLights(Scene* scene, tinyxml2::XMLDocument& document)
	{
		tinyxml2::XMLElement* rootElement = document.RootElement();
		tinyxml2::XMLElement* lightsElement = rootElement ? rootElement->FirstChildElement("Lights") : nullptr;
		if (!scene || !lightsElement)
		{
			return;
		}

		const std::unordered_set<const DirectionalLight*> directionalLights =
			CollectComponentOwnedLights<DirectionalLightComponent, DirectionalLight>(scene);
		RemoveComponentOwnedLightElements(
			lightsElement,
			"DirectionalLight",
			scene->GetDirectionalLights(),
			directionalLights,
			[scene](DirectionalLight* light)
			{
				return scene->GetIsDirectionalLightFromReferencedScene(light);
			});

		const std::unordered_set<const PointLight*> pointLights =
			CollectComponentOwnedLights<PointLightComponent, PointLight>(scene);
		RemoveComponentOwnedLightElements(
			lightsElement,
			"PointLight",
			scene->GetPointLights(),
			pointLights,
			[scene](PointLight* light)
			{
				return scene->GetIsPointLightFromReferencedScene(light);
			});

		const std::unordered_set<const SpotLight*> spotLights =
			CollectComponentOwnedLights<SpotLightComponent, SpotLight>(scene);
		RemoveComponentOwnedLightElements(
			lightsElement,
			"SpotLight",
			scene->GetSpotLights(),
			spotLights,
			[scene](SpotLight* light)
			{
				return scene->GetIsSpotLightFromReferencedScene(light);
			});
	}

	template <typename TLight, typename TMap, typename TIsReferenced>
	void RegisterAuthoredLightDirectionsFromXml(
		tinyxml2::XMLElement* lightsElement,
		const char* elementName,
		const std::vector<TLight*>& lights,
		TIsReferenced isReferenced,
		TMap& authoredDirections)
	{
		tinyxml2::XMLElement* lightElement = lightsElement ? lightsElement->FirstChildElement(elementName) : nullptr;
		for (TLight* light : lights)
		{
			if (!ShouldWriteLight(light, isReferenced(light)))
			{
				continue;
			}

			if (!lightElement)
			{
				break;
			}

			Vector3 direction;
			if (ReadVector3Element(lightElement->FirstChildElement("Direction"), direction))
			{
				authoredDirections[light] = direction;
			}

			lightElement = lightElement->NextSiblingElement(elementName);
		}
	}

	void RegisterAuthoredLightDirectionsFromSceneFile(Scene* scene, const std::string& filePath)
	{
		authoredDirectionalLightDirections.clear();
		authoredSpotLightDirections.clear();

		tinyxml2::XMLDocument document;
		if (!scene || document.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
		{
			return;
		}

		tinyxml2::XMLElement* rootElement = document.RootElement();
		tinyxml2::XMLElement* lightsElement = rootElement ? rootElement->FirstChildElement("Lights") : nullptr;
		if (!lightsElement)
		{
			return;
		}

		RegisterAuthoredLightDirectionsFromXml(
			lightsElement,
			"DirectionalLight",
			scene->GetDirectionalLights(),
			[scene](DirectionalLight* light)
			{
				return scene->GetIsDirectionalLightFromReferencedScene(light);
			},
			authoredDirectionalLightDirections);

		RegisterAuthoredLightDirectionsFromXml(
			lightsElement,
			"SpotLight",
			scene->GetSpotLights(),
			[scene](SpotLight* light)
			{
				return scene->GetIsSpotLightFromReferencedScene(light);
			},
			authoredSpotLightDirections);
	}

	template <typename TLight, typename TMap, typename TIsReferenced>
	void ApplyAuthoredLightDirections(
		tinyxml2::XMLElement* lightsElement,
		const char* elementName,
		const std::vector<TLight*>& lights,
		TIsReferenced isReferenced,
		const TMap& authoredDirections)
	{
		tinyxml2::XMLElement* lightElement = lightsElement ? lightsElement->FirstChildElement(elementName) : nullptr;
		for (TLight* light : lights)
		{
			if (!ShouldWriteLight(light, isReferenced(light)))
			{
				continue;
			}

			if (!lightElement)
			{
				break;
			}

			const auto authoredDirectionIterator = authoredDirections.find(light);
			if (authoredDirectionIterator != authoredDirections.end())
			{
				tinyxml2::XMLElement* directionElement = lightElement->FirstChildElement("Direction");
				if (!directionElement)
				{
					directionElement = lightElement->GetDocument()->NewElement("Direction");
					lightElement->InsertFirstChild(directionElement);
				}

				directionElement->SetText(SerializeVector3(authoredDirectionIterator->second).c_str());
			}

			lightElement = lightElement->NextSiblingElement(elementName);
		}
	}

	void ApplyAuthoredLightDirections(Scene* scene, tinyxml2::XMLDocument& document)
	{
		tinyxml2::XMLElement* rootElement = document.RootElement();
		tinyxml2::XMLElement* lightsElement = rootElement ? rootElement->FirstChildElement("Lights") : nullptr;
		if (!scene || !lightsElement)
		{
			return;
		}

		ApplyAuthoredLightDirections(
			lightsElement,
			"DirectionalLight",
			scene->GetDirectionalLights(),
			[scene](DirectionalLight* light)
			{
				return scene->GetIsDirectionalLightFromReferencedScene(light);
			},
			authoredDirectionalLightDirections);

		ApplyAuthoredLightDirections(
			lightsElement,
			"SpotLight",
			scene->GetSpotLights(),
			[scene](SpotLight* light)
			{
				return scene->GetIsSpotLightFromReferencedScene(light);
			},
			authoredSpotLightDirections);
	}

	void ApplyEditorComponentSerialization(ObjectBase* object, tinyxml2::XMLDocument& document, tinyxml2::XMLElement* componentsElement)
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

			if (ApplyLightComponentSerialization(component, document, componentElement))
			{
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

	void ApplyRegisteredObjectClassNames(Scene* scene, ObjectBase* object, tinyxml2::XMLDocument& document, tinyxml2::XMLElement* objectElement)
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

		ApplyEditorComponentSerialization(object, document, objectElement->FirstChildElement("Components"));

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

			ApplyRegisteredObjectClassNames(scene, childObject, document, childObjectElement);
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

			ApplyRegisteredObjectClassNames(scene, object, document, objectElement);
			objectElement = objectElement->NextSiblingElement();
		}
	}
}

bool EditorSceneSerializer::OpenScene(const std::string& path)
{
	EditorRuntimeDynamicObjectFactoryRegistrar::ClearConstructorOwnedComponentMarkers();
	const bool didOpenScene = engine->GetApplication()->OpenScene(path);
	if (didOpenScene)
	{
		RegisterAuthoredLightDirectionsFromSceneFile(
			engine->GetApplication()->GetMainScene(),
			ContentPathUtils::ToAbsoluteContentPath(path));
	}

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
	ApplyAuthoredLightDirections(scene, document);
	RemoveComponentOwnedLights(scene, document);
	document.SaveFile(filePath.c_str());
}

bool EditorSceneSerializer::GetAuthoredDirection(const DirectionalLight* light, Vector3& outDirection)
{
	const auto authoredDirectionIterator = authoredDirectionalLightDirections.find(light);
	if (authoredDirectionIterator == authoredDirectionalLightDirections.end())
	{
		return false;
	}

	outDirection = authoredDirectionIterator->second;
	return true;
}

bool EditorSceneSerializer::GetAuthoredDirection(const SpotLight* light, Vector3& outDirection)
{
	const auto authoredDirectionIterator = authoredSpotLightDirections.find(light);
	if (authoredDirectionIterator == authoredSpotLightDirections.end())
	{
		return false;
	}

	outDirection = authoredDirectionIterator->second;
	return true;
}

void EditorSceneSerializer::SetAuthoredDirection(const DirectionalLight* light, const Vector3& direction)
{
	if (light)
	{
		authoredDirectionalLightDirections[light] = direction;
	}
}

void EditorSceneSerializer::SetAuthoredDirection(const SpotLight* light, const Vector3& direction)
{
	if (light)
	{
		authoredSpotLightDirections[light] = direction;
	}
}
