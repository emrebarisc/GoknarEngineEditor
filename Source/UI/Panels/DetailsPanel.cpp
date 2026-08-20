#include "DetailsPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Application.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Objects/ReflectionProbeObject.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Components/LightComponents/DirectionalLightComponent.h"
#include "Goknar/Components/LightComponents/PointLightComponent.h"
#include "Goknar/Components/LightComponents/SpotLightComponent.h"
#include "Goknar/Components/ParticleSystemComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Managers/ConfigManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/SkeletalMeshInstance.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/OverlappingPhysicsObject.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/HeightMapCollisionComponent.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Navigation/NavigationTreeComponent.h"
#include "Goknar/Navigation/NavigationTypes.h"
#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"
#include "Goknar/Helpers/SceneParser.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorHUD.h"
#include "UI/EditorRuntimeDynamicObjectFactoryRegistrar.h"
#include "UI/EditorWidgets.h"
#include "UI/Panels/AssetSelectorPanel.h"
#include "UI/Panels/ParticleSystemPanel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

template <class T>
T* AddCollisionComponent(PhysicsObject* physicsObject)
{
	T* existingComponent = physicsObject->GetFirstComponentOfType<T>();
	if (existingComponent)
	{
		return existingComponent;
	}

	return physicsObject->AddSubComponent<T>();
}

namespace
{
	void MarkSceneDirty(const std::string& reason)
	{
		EditorContext::Get()->MarkSceneDirty(reason);
	}

	void RebuildSceneNavigationMesh()
	{
		Scene* scene = engine && engine->GetApplication() ? engine->GetApplication()->GetMainScene() : nullptr;
		if (!scene)
		{
			return;
		}

		const NavMeshSettings defaultSettings;
		scene->RebuildNavigationMesh(defaultSettings);
	}

	bool ObjectHasNavigationTreeComponent(ObjectBase* object)
	{
		if (!object)
		{
			return false;
		}

		if (object->GetFirstComponentOfType<NavigationTreeComponent>())
		{
			return true;
		}

		for (ObjectBase* childObject : object->GetChildren())
		{
			if (ObjectHasNavigationTreeComponent(childObject))
			{
				return true;
			}
		}

		return false;
	}

	bool DrawLightComponentCommonDetails(Light* light, const std::string& specialPostfix)
	{
		if (!light)
		{
			return false;
		}

		bool changed = false;

		float lightIntensity = light->GetIntensity();
		ImGui::Text("Intensity: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##LightComponentIntensity") + specialPostfix, lightIntensity))
		{
			light->SetIntensity(lightIntensity);
			changed = true;
		}

		Vector3 lightColor = light->GetColor();
		ImGui::Text("Color: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputVector3(std::string("##LightComponentColor") + specialPostfix, lightColor))
		{
			light->SetColor(lightColor);
			changed = true;
		}

		bool isShadowEnabled = light->GetIsShadowEnabled();
		ImGui::Text("Cast shadow: ");
		ImGui::SameLine();
		if (ImGui::Checkbox((std::string("##LightComponentIsCastingShadow") + specialPostfix).c_str(), &isShadowEnabled))
		{
			light->SetIsShadowEnabled(isShadowEnabled);
			changed = true;
		}

		float shadowIntensity = light->GetShadowIntensity();
		ImGui::Text("Shadow Intensity: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##LightComponentShadowIntensity") + specialPostfix, shadowIntensity))
		{
			light->SetShadowIntensity(shadowIntensity);
			changed = true;
		}

		return changed;
	}

	enum class MultiObjectDetailsClass
	{
		Mixed,
		ObjectBase,
		PhysicsObject,
		RigidBody,
		OverlappingPhysicsObject
	};

	struct MultiComponentDetailsGroup
	{
		std::string typeName;
		int occurrenceIndex{ 0 };
		std::vector<Component*> components;
	};

	template <typename TEnum>
	struct EnumDisplayValue
	{
		TEnum value;
		const char* label;
	};

	const EnumDisplayValue<CollisionGroup> CollisionGroupValues[]{
		{ CollisionGroup::Default, "Default" },
		{ CollisionGroup::WorldStaticBlock, "WorldStaticBlock" },
		{ CollisionGroup::WorldDynamicBlock, "WorldDynamicBlock" },
		{ CollisionGroup::AllBlock, "AllBlock" },
		{ CollisionGroup::WorldDynamicOverlap, "WorldDynamicOverlap" },
		{ CollisionGroup::WorldStaticOverlap, "WorldStaticOverlap" },
		{ CollisionGroup::AllOverlap, "AllOverlap" },
		{ CollisionGroup::Character, "Character" },
		{ CollisionGroup::All, "All" },
		{ CollisionGroup::Custom0, "Custom0" },
		{ CollisionGroup::Custom1, "Custom1" },
		{ CollisionGroup::Custom2, "Custom2" },
		{ CollisionGroup::Custom3, "Custom3" },
		{ CollisionGroup::Custom4, "Custom4" },
		{ CollisionGroup::Custom5, "Custom5" },
		{ CollisionGroup::Custom6, "Custom6" },
		{ CollisionGroup::Custom7, "Custom7" },
		{ CollisionGroup::Custom8, "Custom8" },
		{ CollisionGroup::Custom9, "Custom9" }
	};

	const EnumDisplayValue<CollisionMask> CollisionMaskValues[]{
		{ CollisionMask::Default, "Default" },
		{ CollisionMask::BlockWorldDynamic, "BlockWorldDynamic" },
		{ CollisionMask::BlockWorldStatic, "BlockWorldStatic" },
		{ CollisionMask::BlockCharacter, "BlockCharacter" },
		{ CollisionMask::BlockAll, "BlockAll" },
		{ CollisionMask::BlockAllExceptCharacter, "BlockAllExceptCharacter" },
		{ CollisionMask::OverlapWorldDynamic, "OverlapWorldDynamic" },
		{ CollisionMask::OverlapWorldStatic, "OverlapWorldStatic" },
		{ CollisionMask::OverlapCharacter, "OverlapCharacter" },
		{ CollisionMask::OverlapAll, "OverlapAll" },
		{ CollisionMask::OverlapAllExceptCharacter, "OverlapAllExceptCharacter" },
		{ CollisionMask::BlockAndOverlapAll, "BlockAndOverlapAll" },
		{ CollisionMask::Custom0, "Custom0" },
		{ CollisionMask::Custom1, "Custom1" },
		{ CollisionMask::Custom2, "Custom2" },
		{ CollisionMask::Custom3, "Custom3" },
		{ CollisionMask::Custom4, "Custom4" },
		{ CollisionMask::Custom5, "Custom5" },
		{ CollisionMask::Custom6, "Custom6" },
		{ CollisionMask::Custom7, "Custom7" },
		{ CollisionMask::Custom8, "Custom8" },
		{ CollisionMask::Custom9, "Custom9" }
	};

	const EnumDisplayValue<CollisionFlag> CollisionFlagValues[]{
		{ CollisionFlag::None, "None" },
		{ CollisionFlag::DynamicObject, "DynamicObject" },
		{ CollisionFlag::StaticObject, "StaticObject" },
		{ CollisionFlag::KinematicObject, "KinematicObject" },
		{ CollisionFlag::NoContactResponse, "NoContactResponse" },
		{ CollisionFlag::CustomMaterialCallback, "CustomMaterialCallback" },
		{ CollisionFlag::CharacterObject, "CharacterObject" },
		{ CollisionFlag::DisableVisualizeObject, "DisableVisualizeObject" },
		{ CollisionFlag::DisableSpuCollisionProcessing, "DisableSpuCollisionProcessing" },
		{ CollisionFlag::HasContactStiffnessDamping, "HasContactStiffnessDamping" },
		{ CollisionFlag::HasCustomDebugRenderingColor, "HasCustomDebugRenderingColor" },
		{ CollisionFlag::HasFrictionAnchor, "HasFrictionAnchor" },
		{ CollisionFlag::HasCollisionSoundTrigger, "HasCollisionSoundTrigger" }
	};

	MultiObjectDetailsClass GetObjectDetailsClass(ObjectBase* object)
	{
		if (dynamic_cast<RigidBody*>(object))
		{
			return MultiObjectDetailsClass::RigidBody;
		}

		if (dynamic_cast<OverlappingPhysicsObject*>(object))
		{
			return MultiObjectDetailsClass::OverlappingPhysicsObject;
		}

		if (dynamic_cast<PhysicsObject*>(object))
		{
			return MultiObjectDetailsClass::PhysicsObject;
		}

		return MultiObjectDetailsClass::ObjectBase;
	}

	MultiObjectDetailsClass GetCommonObjectDetailsClass(const std::vector<ObjectBase*>& objects)
	{
		if (objects.empty())
		{
			return MultiObjectDetailsClass::Mixed;
		}

		const MultiObjectDetailsClass firstClass = GetObjectDetailsClass(objects.front());
		for (ObjectBase* object : objects)
		{
			if (GetObjectDetailsClass(object) != firstClass)
			{
				return MultiObjectDetailsClass::Mixed;
			}
		}

		return firstClass;
	}

	const char* GetObjectDetailsClassName(MultiObjectDetailsClass objectClass)
	{
		switch (objectClass)
		{
		case MultiObjectDetailsClass::ObjectBase:
			return "ObjectBase";
		case MultiObjectDetailsClass::PhysicsObject:
			return "PhysicsObject";
		case MultiObjectDetailsClass::RigidBody:
			return "Rigidbody";
		case MultiObjectDetailsClass::OverlappingPhysicsObject:
			return "OverlappingPhysicsObject";
		case MultiObjectDetailsClass::Mixed:
		default:
			return "Diff";
		}
	}

	bool BatchValuesEqual(float left, float right)
	{
		return GoknarMath::Abs(left - right) <= SMALLER_EPSILON;
	}

	bool BatchValuesEqual(const Vector3& left, const Vector3& right)
	{
		return left.Equals(right, SMALLER_EPSILON);
	}

	template <typename TValue>
	bool BatchValuesEqual(const TValue& left, const TValue& right)
	{
		return left == right;
	}

	template <typename TItem, typename TValue, typename Getter>
	bool GetBatchValueAndDiff(const std::vector<TItem*>& items, Getter getter, TValue& outValue)
	{
		if (items.empty())
		{
			return false;
		}

		outValue = getter(items.front());
		for (TItem* item : items)
		{
			if (!BatchValuesEqual(outValue, getter(item)))
			{
				return true;
			}
		}

		return false;
	}

	void DrawBatchPropertyLabel(const char* label, bool hasDiff)
	{
		ImGui::Text("%s: ", label);
		if (hasDiff)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Diff");
		}
		ImGui::SameLine();
	}

	bool TryParseFloatInput(const char* buffer, float& outValue)
	{
		char* end = nullptr;
		const float value = std::strtof(buffer, &end);
		if (end == buffer)
		{
			return false;
		}

		while (end && *end && std::isspace(static_cast<unsigned char>(*end)))
		{
			++end;
		}

		if (end && *end != '\0')
		{
			return false;
		}

		outValue = value;
		return true;
	}

	bool TryParseIntInput(const char* buffer, int& outValue)
	{
		char* end = nullptr;
		const long value = std::strtol(buffer, &end, 10);
		if (end == buffer)
		{
			return false;
		}

		while (end && *end && std::isspace(static_cast<unsigned char>(*end)))
		{
			++end;
		}

		if (end && *end != '\0')
		{
			return false;
		}

		outValue = static_cast<int>(value);
		return true;
	}

	bool DrawBatchFloatInput(const std::string& id, float& value, bool hasDiff)
	{
		if (!hasDiff)
		{
			return EditorWidgets::DrawInputFloat(id, value);
		}

		char buffer[64]{};
		if (ImGui::InputTextWithHint(id.c_str(), "Diff", buffer, sizeof(buffer)))
		{
			return TryParseFloatInput(buffer, value);
		}

		return false;
	}

	bool DrawBatchIntInput(const std::string& id, int& value, bool hasDiff)
	{
		if (!hasDiff)
		{
			return EditorWidgets::DrawInputInt(id, value);
		}

		char buffer[64]{};
		if (ImGui::InputTextWithHint(id.c_str(), "Diff", buffer, sizeof(buffer)))
		{
			return TryParseIntInput(buffer, value);
		}

		return false;
	}

	template <typename TItem, typename Getter, typename Setter>
	bool DrawBatchStringField(const char* label, const std::string& id, const std::vector<TItem*>& items, Getter getter, Setter setter)
	{
		std::string value;
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);

		char buffer[256]{};
		if (!hasDiff)
		{
			const size_t copyLength = (std::min)(value.size(), sizeof(buffer) - 1);
			std::copy_n(value.c_str(), copyLength, buffer);
		}

		DrawBatchPropertyLabel(label, hasDiff);
		if (ImGui::InputTextWithHint(id.c_str(), hasDiff ? "Diff" : "", buffer, sizeof(buffer)))
		{
			const std::string newValue = buffer;
			for (TItem* item : items)
			{
				setter(item, newValue);
			}
			return true;
		}

		return false;
	}

	template <typename TItem, typename Getter, typename Setter>
	bool DrawBatchBoolField(const char* label, const std::string& id, const std::vector<TItem*>& items, Getter getter, Setter setter)
	{
		bool value = false;
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);

		ImGui::Text("%s: ", label);
		ImGui::SameLine();
		bool changed = false;
		const char* previewValue = hasDiff ? "Diff" : (value ? "True" : "False");
		if (ImGui::BeginCombo(id.c_str(), previewValue))
		{
			if (ImGui::Selectable("False", !hasDiff && !value))
			{
				for (TItem* item : items)
				{
					setter(item, false);
				}
				changed = true;
			}

			if (ImGui::Selectable("True", !hasDiff && value))
			{
				for (TItem* item : items)
				{
					setter(item, true);
				}
				changed = true;
			}

			ImGui::EndCombo();
		}

		return changed;
	}

	template <typename TItem, typename Getter, typename Setter>
	bool DrawBatchFloatField(const char* label, const std::string& id, const std::vector<TItem*>& items, Getter getter, Setter setter)
	{
		float value = 0.f;
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);

		DrawBatchPropertyLabel(label, hasDiff);
		if (DrawBatchFloatInput(id, value, hasDiff))
		{
			for (TItem* item : items)
			{
				setter(item, value);
			}
			return true;
		}

		return false;
	}

	template <typename TItem, typename Getter, typename Setter>
	bool DrawBatchIntField(const char* label, const std::string& id, const std::vector<TItem*>& items, Getter getter, Setter setter)
	{
		int value = 0;
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);

		DrawBatchPropertyLabel(label, hasDiff);
		if (DrawBatchIntInput(id, value, hasDiff))
		{
			for (TItem* item : items)
			{
				setter(item, value);
			}
			return true;
		}

		return false;
	}

	template <typename TItem, typename Getter>
	bool GetBatchVector3AxisValueAndDiff(const std::vector<TItem*>& items, Getter getter, int axis, float& outValue)
	{
		if (items.empty())
		{
			return false;
		}

		outValue = getter(items.front())[axis];
		for (TItem* item : items)
		{
			if (!BatchValuesEqual(outValue, getter(item)[axis]))
			{
				return true;
			}
		}

		return false;
	}

	template <typename TItem, typename Getter, typename Setter>
	bool DrawBatchVector3Field(const char* label, const std::string& id, const std::vector<TItem*>& items, Getter getter, Setter setter)
	{
		float axisValues[3]{ 0.f, 0.f, 0.f };
		bool axisHasDiff[3]{
			GetBatchVector3AxisValueAndDiff(items, getter, 0, axisValues[0]),
			GetBatchVector3AxisValueAndDiff(items, getter, 1, axisValues[1]),
			GetBatchVector3AxisValueAndDiff(items, getter, 2, axisValues[2])
		};

		DrawBatchPropertyLabel(label, axisHasDiff[0] || axisHasDiff[1] || axisHasDiff[2]);

		bool changed = false;
		const char* axisSuffixes[3]{ "X", "Y", "Z" };
		for (int axis = 0; axis < 3; ++axis)
		{
			if (axis > 0)
			{
				ImGui::SameLine();
			}

			float newAxisValue = axisValues[axis];
			if (DrawBatchFloatInput(id + axisSuffixes[axis], newAxisValue, axisHasDiff[axis]))
			{
				for (TItem* item : items)
				{
					Vector3 itemValue = getter(item);
					itemValue[axis] = newAxisValue;
					setter(item, itemValue);
				}
				changed = true;
			}
		}

		return changed;
	}

	template <typename TItem, typename Getter>
	void DrawBatchTextValue(const char* label, const std::vector<TItem*>& items, Getter getter)
	{
		std::string value;
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);

		ImGui::Text("%s: ", label);
		ImGui::SameLine();
		if (hasDiff)
		{
			ImGui::TextDisabled("Diff");
		}
		else
		{
			ImGui::TextWrapped("%s", value.c_str());
		}
	}

	template <typename TEnum, size_t EntryCount>
	int GetEnumEntryIndex(TEnum value, const EnumDisplayValue<TEnum>(&entries)[EntryCount])
	{
		for (int entryIndex = 0; entryIndex < static_cast<int>(EntryCount); ++entryIndex)
		{
			if (entries[entryIndex].value == value)
			{
				return entryIndex;
			}
		}

		return 0;
	}

	template <typename TItem, typename TEnum, typename Getter, typename Setter, size_t EntryCount>
	bool DrawBatchEnumComboField(
		const char* label,
		const std::string& id,
		const std::vector<TItem*>& items,
		Getter getter,
		Setter setter,
		const EnumDisplayValue<TEnum>(&entries)[EntryCount])
	{
		TEnum value{};
		const bool hasDiff = GetBatchValueAndDiff(items, getter, value);
		const int selectedEntryIndex = GetEnumEntryIndex(value, entries);

		ImGui::Text("%s: ", label);
		ImGui::SameLine();

		bool changed = false;
		const char* previewValue = hasDiff ? "Diff" : entries[selectedEntryIndex].label;
		if (ImGui::BeginCombo(id.c_str(), previewValue))
		{
			for (int entryIndex = 0; entryIndex < static_cast<int>(EntryCount); ++entryIndex)
			{
				const bool isSelected = !hasDiff && entryIndex == selectedEntryIndex;
				if (ImGui::Selectable(entries[entryIndex].label, isSelected))
				{
					for (TItem* item : items)
					{
						setter(item, entries[entryIndex].value);
					}
					changed = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		return changed;
	}

	std::string ToDisplayContentPath(const std::string& path)
	{
		if (path.rfind(ContentDir, 0) == 0)
		{
			return path.substr(ContentDir.size());
		}

		return path;
	}

	bool LooksLikeRawFloatHeightMap(const Image* image)
	{
		if (!image || !image->GetBuffer() || image->GetChannels() != static_cast<int>(sizeof(float)))
		{
			return false;
		}

		const unsigned char* buffer = image->GetBuffer();
		const int pixelCount = image->GetWidth() * image->GetHeight();
		if (pixelCount <= 0)
		{
			return false;
		}

		int grayscaleLikePixels = 0;
		for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
		{
			const unsigned char* pixel = buffer + pixelIndex * image->GetChannels();
			if (pixel[0] == pixel[1] && pixel[1] == pixel[2])
			{
				++grayscaleLikePixels;
			}
		}

		return grayscaleLikePixels < pixelCount * 9 / 10;
	}

	float DecodeHeightMapPixel(const unsigned char* pixel, int channels)
	{
		if (!pixel || channels <= 0)
		{
			return 0.0f;
		}

		if (channels == 1 || channels == 2)
		{
			return static_cast<float>(pixel[0]) / 255.0f;
		}

		const float red = static_cast<float>(pixel[0]) / 255.0f;
		const float green = static_cast<float>(pixel[1]) / 255.0f;
		const float blue = static_cast<float>(pixel[2]) / 255.0f;
		return red * 0.2126f + green * 0.7152f + blue * 0.0722f;
	}

	Image* CreateHeightMapCollisionImage(const Image* sourceImage, const HeightMapCollisionComponent* component)
	{
		if (!sourceImage || !sourceImage->GetBuffer())
		{
			return nullptr;
		}

		if (LooksLikeRawFloatHeightMap(sourceImage))
		{
			return const_cast<Image*>(sourceImage);
		}

		const int width = sourceImage->GetWidth();
		const int height = sourceImage->GetHeight();
		const int channels = sourceImage->GetChannels();
		if (width <= 1 || height <= 1 || channels <= 0)
		{
			return nullptr;
		}

		float heightScale = component ? component->GetMaxHeight() : 1.0f;
		if (!std::isfinite(heightScale) || heightScale <= SMALLER_EPSILON)
		{
			heightScale = 1.0f;
		}

		const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
		unsigned char* floatBuffer = new unsigned char[pixelCount * sizeof(float)];
		const unsigned char* sourceBuffer = sourceImage->GetBuffer();
		for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
		{
			const float heightValue = DecodeHeightMapPixel(sourceBuffer + pixelIndex * channels, channels) * heightScale;
			std::memcpy(floatBuffer + pixelIndex * sizeof(float), &heightValue, sizeof(float));
		}

		static std::vector<std::unique_ptr<Image>> ownedCollisionImages;
		std::unique_ptr<Image> collisionImage = std::make_unique<Image>(
			sourceImage->GetPath(),
			width,
			height,
			static_cast<int>(sizeof(float)),
			floatBuffer);
		collisionImage->SetName(sourceImage->GetName());
		collisionImage->SetTextureUsage(TextureUsage::Height);
		collisionImage->SetCanUseTextureAtlas(false);

		Image* collisionImagePtr = collisionImage.get();
		ownedCollisionImages.push_back(std::move(collisionImage));
		return collisionImagePtr;
	}

	std::string GetComponentTypeName(Component* component)
	{
		if (dynamic_cast<StaticMeshParticleSystemComponent*>(component))
		{
			return "StaticMeshParticleSystemComponent";
		}

		if (dynamic_cast<BillboardParticleSystemComponent*>(component))
		{
			return "BillboardParticleSystemComponent";
		}

		if (dynamic_cast<DirectionalLightComponent*>(component))
		{
			return "DirectionalLightComponent";
		}

		if (dynamic_cast<PointLightComponent*>(component))
		{
			return "PointLightComponent";
		}

		if (dynamic_cast<SpotLightComponent*>(component))
		{
			return "SpotLightComponent";
		}

		if (dynamic_cast<StaticMeshComponent*>(component))
		{
			return "StaticMeshComponent";
		}

		if (dynamic_cast<SkeletalMeshComponent*>(component))
		{
			return "SkeletalMeshComponent";
		}

		if (dynamic_cast<BoxCollisionComponent*>(component))
		{
			return "BoxCollisionComponent";
		}

		if (dynamic_cast<SphereCollisionComponent*>(component))
		{
			return "SphereCollisionComponent";
		}

		if (dynamic_cast<CapsuleCollisionComponent*>(component))
		{
			return "CapsuleCollisionComponent";
		}

		if (dynamic_cast<HeightMapCollisionComponent*>(component))
		{
			return "HeightMapCollisionComponent";
		}

		if (dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
		{
			return "MovingTriangleMeshCollisionComponent";
		}

		if (dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
		{
			return "NonMovingTriangleMeshCollisionComponent";
		}

		if (dynamic_cast<NavigationTreeComponent*>(component))
		{
			return "NavigationTreeComponent";
		}

		return "Component";
	}

	std::vector<Component*> GetComponentsByTypeName(ObjectBase* object, const std::string& typeName)
	{
		std::vector<Component*> components;
		if (!object)
		{
			return components;
		}

		for (Component* component : object->GetComponents())
		{
			if (GetComponentTypeName(component) == typeName)
			{
				components.push_back(component);
			}
		}

		return components;
	}

	std::vector<MultiComponentDetailsGroup> GetMutualComponentDetailsGroups(const std::vector<ObjectBase*>& objects)
	{
		std::vector<MultiComponentDetailsGroup> groups;
		if (objects.empty())
		{
			return groups;
		}

		std::map<std::string, int> occurrenceCountsByType;
		for (Component* firstObjectComponent : objects.front()->GetComponents())
		{
			const std::string typeName = GetComponentTypeName(firstObjectComponent);
			const int occurrenceIndex = occurrenceCountsByType[typeName]++;

			MultiComponentDetailsGroup group;
			group.typeName = typeName;
			group.occurrenceIndex = occurrenceIndex;
			group.components.push_back(firstObjectComponent);

			bool isMutualComponent = true;
			for (size_t objectIndex = 1; objectIndex < objects.size(); ++objectIndex)
			{
				const std::vector<Component*> matchingComponents = GetComponentsByTypeName(objects[objectIndex], typeName);
				if (occurrenceIndex >= static_cast<int>(matchingComponents.size()))
				{
					isMutualComponent = false;
					break;
				}

				group.components.push_back(matchingComponents[occurrenceIndex]);
			}

			if (isMutualComponent)
			{
				groups.push_back(group);
			}
		}

		return groups;
	}

	template <typename TTarget, typename TSource>
	std::vector<TTarget*> CastItems(const std::vector<TSource*>& items)
	{
		std::vector<TTarget*> castItems;
		for (TSource* item : items)
		{
			TTarget* castItem = dynamic_cast<TTarget*>(item);
			if (!castItem)
			{
				castItems.clear();
				return castItems;
			}

			castItems.push_back(castItem);
		}

		return castItems;
	}

	template <typename TComponent>
	bool DrawBatchLightComponentCommonFields(const char* idPrefix, const std::vector<TComponent*>& components)
	{
		const std::string id = std::string("##Multi") + idPrefix;
		bool changed = false;

		changed |= DrawBatchFloatField(
			"Intensity",
			id + "Intensity",
			components,
			[](TComponent* component)
			{
				const auto* light = component ? component->GetLight() : nullptr;
				return light ? light->GetIntensity() : 0.f;
			},
			[](TComponent* component, float value)
			{
				if (auto* light = component ? component->GetLight() : nullptr)
				{
					light->SetIntensity(value);
				}
			});

		changed |= DrawBatchVector3Field(
			"Color",
			id + "Color",
			components,
			[](TComponent* component)
			{
				const auto* light = component ? component->GetLight() : nullptr;
				return light ? light->GetColor() : Vector3::ZeroVector;
			},
			[](TComponent* component, const Vector3& value)
			{
				if (auto* light = component ? component->GetLight() : nullptr)
				{
					light->SetColor(value);
				}
			});

		changed |= DrawBatchBoolField(
			"Cast shadow",
			id + "IsCastingShadow",
			components,
			[](TComponent* component)
			{
				const auto* light = component ? component->GetLight() : nullptr;
				return light ? light->GetIsShadowEnabled() : false;
			},
			[](TComponent* component, bool value)
			{
				if (auto* light = component ? component->GetLight() : nullptr)
				{
					light->SetIsShadowEnabled(value);
				}
			});

		changed |= DrawBatchFloatField(
			"Shadow Intensity",
			id + "ShadowIntensity",
			components,
			[](TComponent* component)
			{
				const auto* light = component ? component->GetLight() : nullptr;
				return light ? light->GetShadowIntensity() : 0.f;
			},
			[](TComponent* component, float value)
			{
				if (auto* light = component ? component->GetLight() : nullptr)
				{
					light->SetShadowIntensity(value);
				}
			});

		return changed;
	}

	const char* DetailsCollisionDebugRootName = "__DetailsCollisionDebug__";

	bool IsDetailsCollisionDebugRoot(const DebugObject* debugObject)
	{
		return debugObject && debugObject->GetNameWithoutId() == DetailsCollisionDebugRootName;
	}

	std::vector<DebugObject*> GetDetailsCollisionDebugRoots(ObjectBase* object)
	{
		std::vector<DebugObject*> debugRoots;
		if (!engine || !object)
		{
			return debugRoots;
		}

		const std::vector<DebugObject*> debugObjects = engine->GetObjectsOfType<DebugObject>();
		for (DebugObject* debugObject : debugObjects)
		{
			if (IsDetailsCollisionDebugRoot(debugObject) && debugObject->GetParent() == object)
			{
				debugRoots.push_back(debugObject);
			}
		}

		return debugRoots;
	}

	bool IsDetailsCollisionDebugEnabled(ObjectBase* object)
	{
		return !GetDetailsCollisionDebugRoots(object).empty();
	}

	bool IsDrawableCollisionComponent(const Component* component)
	{
		return dynamic_cast<const BoxCollisionComponent*>(component) ||
			dynamic_cast<const SphereCollisionComponent*>(component) ||
			dynamic_cast<const CapsuleCollisionComponent*>(component) ||
			dynamic_cast<const MovingTriangleMeshCollisionComponent*>(component) ||
			dynamic_cast<const NonMovingTriangleMeshCollisionComponent*>(component);
	}

	bool ObjectHasDrawableCollisionComponent(ObjectBase* object)
	{
		if (!object)
		{
			return false;
		}

		for (Component* component : object->GetComponents())
		{
			if (IsDrawableCollisionComponent(component))
			{
				return true;
			}
		}

		return false;
	}

	void ClearDetailsCollisionDebug(ObjectBase* object)
	{
		for (DebugObject* debugRoot : GetDetailsCollisionDebugRoots(object))
		{
			if (debugRoot)
			{
				debugRoot->Destroy();
			}
		}
	}

	void DrawTriangleMeshCollisionDebug(
		const StaticMesh* mesh,
		const Vector3& position,
		const Quaternion& rotation,
		const Vector3& scaling,
		DebugObject* debugRoot)
	{
		if (!mesh || mesh->GetSubMeshes().empty() || !debugRoot)
		{
			return;
		}

		DebugObject* collisionObject = new DebugObject();
		collisionObject->SetName("DebugObject_TriangleMeshCollisionComponent");
		DebugDrawer::DrawMeshUnit(mesh->GetSubMeshes()[0], Colorf::Blue, 1.f, -1.f, collisionObject);
		collisionObject->SetWorldPosition(position, false);
		collisionObject->SetWorldRotation(rotation, false);
		collisionObject->SetWorldScaling(scaling);
		collisionObject->SetParent(debugRoot);
	}

	void DrawCollisionComponentDebug(const Component* component, DebugObject* debugRoot)
	{
		if (!component || !debugRoot)
		{
			return;
		}

		if (const BoxCollisionComponent* boxCollisionComponent = dynamic_cast<const BoxCollisionComponent*>(component))
		{
			DebugDrawer::DrawBox(
				boxCollisionComponent->GetWorldPosition(),
				boxCollisionComponent->GetWorldRotation(),
				boxCollisionComponent->GetHalfSize() * boxCollisionComponent->GetWorldScaling(),
				Colorf::White,
				1.f,
				-1.f,
				debugRoot);
			return;
		}

		if (const SphereCollisionComponent* sphereCollisionComponent = dynamic_cast<const SphereCollisionComponent*>(component))
		{
			DebugDrawer::DrawSphere(
				sphereCollisionComponent->GetWorldPosition(),
				sphereCollisionComponent->GetWorldRotation(),
				sphereCollisionComponent->GetRadius(),
				Colorf::Blue,
				1.f,
				-1.f,
				debugRoot);
			return;
		}

		if (const CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<const CapsuleCollisionComponent*>(component))
		{
			DebugDrawer::DrawCapsule(
				capsuleCollisionComponent->GetWorldPosition(),
				capsuleCollisionComponent->GetWorldRotation(),
				capsuleCollisionComponent->GetRadius(),
				capsuleCollisionComponent->GetHeight(),
				Colorf::Blue,
				1.f,
				-1.f,
				debugRoot);
			return;
		}

		if (const MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<const MovingTriangleMeshCollisionComponent*>(component))
		{
			DrawTriangleMeshCollisionDebug(
				movingTriangleMeshCollisionComponent->GetMesh(),
				movingTriangleMeshCollisionComponent->GetWorldPosition(),
				movingTriangleMeshCollisionComponent->GetWorldRotation(),
				movingTriangleMeshCollisionComponent->GetWorldScaling(),
				debugRoot);
			return;
		}

		if (const NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent = dynamic_cast<const NonMovingTriangleMeshCollisionComponent*>(component))
		{
			DrawTriangleMeshCollisionDebug(
				nonMovingTriangleMeshCollisionComponent->GetMesh(),
				nonMovingTriangleMeshCollisionComponent->GetWorldPosition(),
				nonMovingTriangleMeshCollisionComponent->GetWorldRotation(),
				nonMovingTriangleMeshCollisionComponent->GetWorldScaling(),
				debugRoot);
		}
	}

	void DrawDetailsCollisionDebug(ObjectBase* object)
	{
		if (!object || IsDetailsCollisionDebugEnabled(object))
		{
			return;
		}

		DebugObject* debugRoot = new DebugObject();
		debugRoot->SetName(DetailsCollisionDebugRootName);
		debugRoot->SetParent(object);

		for (Component* component : object->GetComponents())
		{
			DrawCollisionComponentDebug(component, debugRoot);
		}
	}

	void RefreshDetailsCollisionDebug(ObjectBase* object)
	{
		if (!IsDetailsCollisionDebugEnabled(object))
		{
			return;
		}

		ClearDetailsCollisionDebug(object);
		DrawDetailsCollisionDebug(object);
	}

	void DrawDetailsCollisionDebugToggle(ObjectBase* object)
	{
		if (!ObjectHasDrawableCollisionComponent(object))
		{
			return;
		}

		bool drawCollisionDebug = IsDetailsCollisionDebugEnabled(object);
		ImGui::Text("Collision Debug: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##CollisionDebug", &drawCollisionDebug))
		{
			if (drawCollisionDebug)
			{
				DrawDetailsCollisionDebug(object);
			}
			else
			{
				ClearDetailsCollisionDebug(object);
			}
		}
	}
}

DetailsPanel::DetailsPanel(EditorHUD* hud) : IEditorPanel("Details", hud)
{
	SetupReflections();
}

void DetailsPanel::SetupReflections()
{
	objectReflections_["StaticMeshComponent"] = [](ObjectBase* o) {
		o->AddSubComponent<StaticMeshComponent>();
		};

	objectReflections_["StaticMeshComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<StaticMeshComponent>();
		};

	objectReflections_["SkeletalMeshComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<SkeletalMeshComponent>();
		};

	objectReflections_["BillboardParticleSystemComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<BillboardParticleSystemComponent>();
		};

	objectReflections_["StaticMeshParticleSystemComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<StaticMeshParticleSystemComponent>();
		};

	objectReflections_["DirectionalLightComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<DirectionalLightComponent>();
		};

	objectReflections_["PointLightComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<PointLightComponent>();
		};

	objectReflections_["SpotLightComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<SpotLightComponent>();
		};

	objectReflections_["NavigationTreeComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<NavigationTreeComponent>();
		};

	physicsReflections_["BoxCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			if (physicsObject->GetFirstComponentOfType<BoxCollisionComponent>())
			{
				return;
			}

			BoxCollisionComponent* boxCollisionComponent = physicsObject->AddSubComponent<BoxCollisionComponent>();
			if (!dynamic_cast<RigidBody*>(physicsObject))
			{
				return;
			}

			StaticMeshComponent* staticMeshComponent = physicsObject->GetFirstComponentOfType<StaticMeshComponent>();
			StaticMesh* staticMesh = staticMeshComponent && staticMeshComponent->GetMeshInstance() ? staticMeshComponent->GetMeshInstance()->GetMesh() : nullptr;
			if (staticMesh && staticMesh->GetAABB().GetSize() != Vector3::ZeroVector)
			{
				boxCollisionComponent->SetHalfSize(staticMesh->GetAABB().GetSize() * 0.5f);
			}
		};
	physicsReflections_["CapsuleCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<CapsuleCollisionComponent>(physicsObject);
		};
	physicsReflections_["SphereCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<SphereCollisionComponent>(physicsObject);
		};
	physicsReflections_["HeightMapCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<HeightMapCollisionComponent>(physicsObject);
		};
	physicsReflections_["MovingTriangleMeshCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = AddCollisionComponent<MovingTriangleMeshCollisionComponent>(physicsObject);

			StaticMeshComponent* staticMeshComponent = physicsObject->GetFirstComponentOfType<StaticMeshComponent>();
			if (staticMeshComponent)
			{
				movingTriangleMeshCollisionComponent->SetMesh(staticMeshComponent->GetMeshInstance()->GetMesh());
			}
		};
	physicsReflections_["NonMovingTriangleMeshCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent = AddCollisionComponent<NonMovingTriangleMeshCollisionComponent>(physicsObject);

			StaticMeshComponent* staticMeshComponent = physicsObject->GetFirstComponentOfType<StaticMeshComponent>();
			if (staticMeshComponent)
			{
				nonMovingTriangleMeshCollisionComponent->SetMesh(staticMeshComponent->GetMeshInstance()->GetMesh());
			}
		};
}

void DetailsPanel::OnAssetSelected(const std::string& path)
{
	std::string normalizedPath = EditorAssetPathUtils::ToContentRelativePath(path);
	std::vector<void*> assetSelectionComponents = assetSelectionComponents_;
	if (assetSelectionComponents.empty() && assetSelectionComponent_)
	{
		assetSelectionComponents.push_back(assetSelectionComponent_);
	}
	assetSelectionComponents.erase(
		std::remove_if(
			assetSelectionComponents.begin(),
			assetSelectionComponents.end(),
			[](void* component)
			{
				return EditorRuntimeDynamicObjectFactoryRegistrar::IsConstructorOwnedComponent(static_cast<Component*>(component));
			}),
		assetSelectionComponents.end());
	if (assetSelectionComponents.empty())
	{
		assetSelectionComponent_ = nullptr;
		assetSelectionComponents_.clear();
		assetSelectionComponentType_ = DetailsAssetSelectionTarget::None;
		assetSelectionSubMeshIndex_ = -1;
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;
		return;
	}

	switch (assetSelectionComponentType_)
	{
	case DetailsAssetSelectionTarget::None:
		break;
	case DetailsAssetSelectionTarget::StaticMesh:
	{
		StaticMesh* newStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(normalizedPath);
		if (newStaticMesh)
		{
			for (void* assetSelectionComponent : assetSelectionComponents)
			{
				StaticMeshComponent* staticMeshComponent = (StaticMeshComponent*)assetSelectionComponent;
				if (staticMeshComponent)
				{
					staticMeshComponent->SetMesh(newStaticMesh);
					staticMeshComponent->PreInit();
					staticMeshComponent->Init();
					staticMeshComponent->PostInit();
				}
			}
			MarkSceneDirty("Static mesh changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::Material:
	{
		for (void* assetSelectionComponent : assetSelectionComponents)
		{
			StaticMeshComponent* staticMeshComponent = (StaticMeshComponent*)assetSelectionComponent;
			std::vector<std::string> materialPaths = SceneParser::GetStaticMeshComponentMaterialPaths(staticMeshComponent);

			StaticMeshInstance* meshInstance = staticMeshComponent ? staticMeshComponent->GetMeshInstance() : nullptr;
			StaticMesh* mesh = meshInstance ? meshInstance->GetMesh() : nullptr;
			const size_t subMeshCount = mesh ? mesh->GetSubMeshes().size() : 0;

			if (assetSelectionSubMeshIndex_ >= 0 && assetSelectionSubMeshIndex_ < static_cast<int>(subMeshCount))
			{
				materialPaths.resize(subMeshCount);
				materialPaths[assetSelectionSubMeshIndex_] = normalizedPath;
				SceneParser::ApplyStaticMeshComponentMaterialPaths(staticMeshComponent, materialPaths);
				MarkSceneDirty("Material changed");
			}
		}
		break;
	}
	case DetailsAssetSelectionTarget::SkeletalMesh:
	{
		SkeletalMesh* newSkeletalMesh = engine->GetResourceManager()->GetContent<SkeletalMesh>(normalizedPath);
		if (newSkeletalMesh)
		{
			for (void* assetSelectionComponent : assetSelectionComponents)
			{
				SkeletalMeshComponent* skeletalMeshComponent = (SkeletalMeshComponent*)assetSelectionComponent;
				if (skeletalMeshComponent)
				{
					skeletalMeshComponent->SetMesh(newSkeletalMesh);
					skeletalMeshComponent->PreInit();
					skeletalMeshComponent->Init();
					skeletalMeshComponent->PostInit();
				}
			}
			MarkSceneDirty("Skeletal mesh changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::SkeletalMeshMaterial:
	{
		for (void* assetSelectionComponent : assetSelectionComponents)
		{
			SkeletalMeshComponent* skeletalMeshComponent = (SkeletalMeshComponent*)assetSelectionComponent;
			std::vector<std::string> materialPaths = SceneParser::GetSkeletalMeshComponentMaterialPaths(skeletalMeshComponent);

			SkeletalMeshInstance* meshInstance = skeletalMeshComponent ? skeletalMeshComponent->GetMeshInstance() : nullptr;
			SkeletalMesh* mesh = meshInstance ? meshInstance->GetMesh() : nullptr;
			const size_t subMeshCount = mesh ? mesh->GetSubMeshes().size() : 0;

			if (assetSelectionSubMeshIndex_ >= 0 && assetSelectionSubMeshIndex_ < static_cast<int>(subMeshCount))
			{
				materialPaths.resize(subMeshCount);
				materialPaths[assetSelectionSubMeshIndex_] = normalizedPath;
				SceneParser::ApplySkeletalMeshComponentMaterialPaths(skeletalMeshComponent, materialPaths);
				MarkSceneDirty("Skeletal mesh material changed");
			}
		}
		break;
	}
	case DetailsAssetSelectionTarget::NavigationTree:
	{
		bool changedNavigationTree = false;
		for (void* assetSelectionComponent : assetSelectionComponents)
		{
			NavigationTreeComponent* navigationTreeComponent = (NavigationTreeComponent*)assetSelectionComponent;
			if (navigationTreeComponent)
			{
				const std::string previousPath = navigationTreeComponent->GetNavigationTreePath();
				const bool loadedNavigationTree = navigationTreeComponent->SetNavigationTreePath(normalizedPath);
				changedNavigationTree |= loadedNavigationTree && previousPath != navigationTreeComponent->GetNavigationTreePath();
			}
		}
		if (changedNavigationTree)
		{
			RebuildSceneNavigationMesh();
			MarkSceneDirty("Navigation tree changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::HeightMapImage:
	{
		Image* newHeightMapImage = engine->GetResourceManager()->GetContent<Image>(normalizedPath);
		if (newHeightMapImage)
		{
			for (void* assetSelectionComponent : assetSelectionComponents)
			{
				HeightMapCollisionComponent* heightMapCollisionComponent = (HeightMapCollisionComponent*)assetSelectionComponent;
				if (heightMapCollisionComponent)
				{
					Image* collisionImage = CreateHeightMapCollisionImage(newHeightMapImage, heightMapCollisionComponent);
					heightMapCollisionComponent->SetHeightMapImage(collisionImage ? collisionImage : newHeightMapImage);
					if (newHeightMapImage->GetWidth() > 1)
					{
						heightMapCollisionComponent->SetHeightStickWidth(newHeightMapImage->GetWidth());
					}
					if (newHeightMapImage->GetHeight() > 1)
					{
						heightMapCollisionComponent->SetHeightStickLength(newHeightMapImage->GetHeight());
					}
				}
			}
			MarkSceneDirty("Height map image changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::Image:
		break;
	case DetailsAssetSelectionTarget::Audio:
		break;
	default:
		break;
	}

	assetSelectionComponent_ = nullptr;
	assetSelectionComponents_.clear();
	assetSelectionComponentType_ = DetailsAssetSelectionTarget::None;
	assetSelectionSubMeshIndex_ = -1;
	EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;
}

void DetailsPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	switch (EditorContext::Get()->selectedObjectType)
	{
	case EditorSelectionType::None:
		break;
	case EditorSelectionType::Object:
		if (EditorContext::Get()->GetSelectedObjects().size() > 1)
		{
			DrawMultipleObjectDetails();
		}
		else
		{
			DrawObjectDetails();
		}
		break;
	case EditorSelectionType::DirectionalLight:
		DrawDirectionalLightDetails();
		break;
	case EditorSelectionType::PointLight:
		DrawPointLightDetails();
		break;
	case EditorSelectionType::SpotLight:
		DrawSpotLightDetails();
		break;
	default:
		break;
	}

	ImGui::End();
}

void DetailsPanel::DrawTransform()
{
	ObjectBase* object = EditorContext::Get()->GetSelectionAs<ObjectBase>();

	Vector3 position = object->GetWorldPosition();
	Vector3 rotation = object->GetWorldRotation().ToEulerDegrees();
	Vector3 scale = object->GetWorldScaling();

	ImGui::Text("Position: ");
	ImGui::SameLine();
	const bool positionChanged = EditorWidgets::DrawInputVector3("##Position", position);
	if (positionChanged)
	{
		object->SetWorldPosition(position);
	}

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	const bool rotationChanged = EditorWidgets::DrawInputVector3("##Rotation", rotation);
	if (rotationChanged)
	{
		object->SetWorldRotation(Quaternion::FromEulerDegrees(rotation));
	}

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	const bool scalingChanged = EditorWidgets::DrawInputVector3("##Scaling", scale);
	if (scalingChanged)
	{
		object->SetWorldScaling(scale);
	}

	if (positionChanged || rotationChanged || scalingChanged)
	{
		MarkSceneDirty("Object transform changed");
	}
}

void DetailsPanel::DrawObjectDetails()
{
	ObjectBase* object = EditorContext::Get()->GetSelectionAs<ObjectBase>();

	Vector3 selectedObjectWorldPosition = object->GetWorldPosition();
	Vector3 selectedObjectWorldRotationEulerDegrees = object->GetWorldRotation().ToEulerDegrees();
	Vector3 selectedObjectWorldScaling = object->GetWorldScaling();
	std::string selectedObjectName = object->GetNameWithoutId();

	ImGui::Text(selectedObjectName.c_str());
	ImGui::Separator();

	ImGui::Text("Add Component: ");
	ImGui::SameLine();

	DrawAddComponentOptions(object);

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Name: ");
	ImGui::SameLine();
	std::string newName = selectedObjectName;
	const bool nameChanged = EditorWidgets::DrawInputText("##Name", newName);
	if (newName != selectedObjectName)
	{
		object->SetName(newName);
		MarkSceneDirty("Object name changed");
	}

	ImGui::Text("Position: ");
	ImGui::SameLine();
	const bool positionChanged = EditorWidgets::DrawInputVector3("##Position", selectedObjectWorldPosition);
	if (positionChanged)
	{
		object->SetWorldPosition(selectedObjectWorldPosition);
	}

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	const bool rotationChanged = EditorWidgets::DrawInputVector3("##Rotation", selectedObjectWorldRotationEulerDegrees);
	if(rotationChanged)
	{
		object->SetWorldRotation(Quaternion::FromEulerDegrees(selectedObjectWorldRotationEulerDegrees));
	}

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	const bool scalingChanged = EditorWidgets::DrawInputVector3("##Scaling", selectedObjectWorldScaling);
	if (scalingChanged)
	{
		object->SetWorldScaling(selectedObjectWorldScaling);
	}

	if (positionChanged || rotationChanged || scalingChanged)
	{
		if (ObjectHasNavigationTreeComponent(object))
		{
			RebuildSceneNavigationMesh();
		}

		if (!nameChanged)
		{
			MarkSceneDirty("Object transform changed");
		}
	}

	DrawDetailsCollisionDebugToggle(object);

	ImGui::Separator();
	ReflectionProbeObject* reflectionProbeObject = dynamic_cast<ReflectionProbeObject*>(object);
	if (reflectionProbeObject)
	{
		DrawReflectionProbeObjectDetails(reflectionProbeObject);
		ImGui::Separator();
	}

	RigidBody* rigidBody = dynamic_cast<RigidBody*>(object);
	if (rigidBody)
	{
		float rigidBodyMass = rigidBody->GetMass();
		ImGui::Text("Mass: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat("##Mass", rigidBodyMass))
		{
			rigidBody->SetMass(rigidBodyMass);
			MarkSceneDirty("Rigid body mass changed");
		}

		static const char* collisionGroupNames[]{
			"",
			"Default",
			"WorldStaticBlock",
			"WorldDynamicBlock",
			"AllBlock",
			"WorldDynamicOverlap",
			"WorldStaticOverlap",
			"AllOverlap",
			"Character",
			"All",
			"Custom0",
			"Custom1",
			"Custom2",
			"Custom3",
			"Custom4",
			"Custom5",
			"Custom6",
			"Custom7",
			"Custom8",
			"Custom9"
		};

		int selectedCollisionGroup = 1;
		CollisionGroup collisionGroup = rigidBody->GetCollisionGroup();
		if (collisionGroup == CollisionGroup::Default) selectedCollisionGroup = 1;
		else if (collisionGroup == CollisionGroup::WorldStaticBlock) selectedCollisionGroup = 2;
		else if (collisionGroup == CollisionGroup::WorldDynamicBlock) selectedCollisionGroup = 3;
		else if (collisionGroup == CollisionGroup::AllBlock) selectedCollisionGroup = 4;
		else if (collisionGroup == CollisionGroup::WorldDynamicOverlap) selectedCollisionGroup = 5;
		else if (collisionGroup == CollisionGroup::WorldStaticOverlap) selectedCollisionGroup = 6;
		else if (collisionGroup == CollisionGroup::AllOverlap) selectedCollisionGroup = 7;
		else if (collisionGroup == CollisionGroup::Character) selectedCollisionGroup = 8;
		else if (collisionGroup == CollisionGroup::All) selectedCollisionGroup = 9;
		else if (collisionGroup == CollisionGroup::Custom0) selectedCollisionGroup = 10;
		else if (collisionGroup == CollisionGroup::Custom1) selectedCollisionGroup = 11;
		else if (collisionGroup == CollisionGroup::Custom2) selectedCollisionGroup = 12;
		else if (collisionGroup == CollisionGroup::Custom3) selectedCollisionGroup = 13;
		else if (collisionGroup == CollisionGroup::Custom4) selectedCollisionGroup = 14;
		else if (collisionGroup == CollisionGroup::Custom5) selectedCollisionGroup = 15;
		else if (collisionGroup == CollisionGroup::Custom6) selectedCollisionGroup = 16;
		else if (collisionGroup == CollisionGroup::Custom7) selectedCollisionGroup = 17;
		else if (collisionGroup == CollisionGroup::Custom8) selectedCollisionGroup = 18;
		else if (collisionGroup == CollisionGroup::Custom9) selectedCollisionGroup = 19;

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, 125);
		ImGui::Text("CollisionGroup: ");
		ImGui::NextColumn();
		ImGui::SetColumnWidth(1, 800);
		bool collisionGroupCheck = ImGui::Combo("##CollisionGroup", &selectedCollisionGroup, collisionGroupNames, IM_ARRAYSIZE(collisionGroupNames));
		if (collisionGroupCheck)
		{
			CollisionGroup newCollisionGroup;
			if (selectedCollisionGroup == 2)
			{
				newCollisionGroup = CollisionGroup::WorldStaticBlock;
			}
			else if (selectedCollisionGroup == 3)
			{
				newCollisionGroup = CollisionGroup::WorldDynamicBlock;
			}
			else if (selectedCollisionGroup == 4)
			{
				newCollisionGroup = CollisionGroup::AllBlock;
			}
			else if (selectedCollisionGroup == 5)
			{
				newCollisionGroup = CollisionGroup::WorldDynamicOverlap;
			}
			else if (selectedCollisionGroup == 6)
			{
				newCollisionGroup = CollisionGroup::WorldStaticOverlap;
			}
			else if (selectedCollisionGroup == 7)
			{
				newCollisionGroup = CollisionGroup::AllOverlap;
			}
			else if (selectedCollisionGroup == 8)
			{
				newCollisionGroup = CollisionGroup::Character;
			}
			else if (selectedCollisionGroup == 9)
			{
				newCollisionGroup = CollisionGroup::All;
			}
			else if (selectedCollisionGroup == 10)
			{
				newCollisionGroup = CollisionGroup::Custom0;
			}
			else if (selectedCollisionGroup == 11)
			{
				newCollisionGroup = CollisionGroup::Custom1;
			}
			else if (selectedCollisionGroup == 12)
			{
				newCollisionGroup = CollisionGroup::Custom2;
			}
			else if (selectedCollisionGroup == 13)
			{
				newCollisionGroup = CollisionGroup::Custom3;
			}
			else if (selectedCollisionGroup == 14)
			{
				newCollisionGroup = CollisionGroup::Custom4;
			}
			else if (selectedCollisionGroup == 15)
			{
				newCollisionGroup = CollisionGroup::Custom5;
			}
			else if (selectedCollisionGroup == 16)
			{
				newCollisionGroup = CollisionGroup::Custom6;
			}
			else if (selectedCollisionGroup == 17)
			{
				newCollisionGroup = CollisionGroup::Custom7;
			}
			else if (selectedCollisionGroup == 18)
			{
				newCollisionGroup = CollisionGroup::Custom8;
			}
			else if (selectedCollisionGroup == 19)
			{
				newCollisionGroup = CollisionGroup::Custom9;
			}
			else
			{
				newCollisionGroup = CollisionGroup::Default;
			}
			rigidBody->SetCollisionGroup(newCollisionGroup);
			MarkSceneDirty("Rigid body collision group changed");
		}
		ImGui::Columns(1, nullptr, false);

		static const char* collisionMaskNames[]{
			"",
			"Default",
			"BlockWorldDynamic",
			"BlockWorldStatic",
			"BlockCharacter",
			"BlockAll",
			"BlockAllExceptCharacter",
			"OverlapWorldDynamic",
			"OverlapWorldStatic",
			"OverlapCharacter",
			"OverlapAll",
			"OverlapAllExceptCharacter",
			"BlockAndOverlapAll",
			"Custom0",
			"Custom1",
			"Custom2",
			"Custom3",
			"Custom4",
			"Custom5",
			"Custom6",
			"Custom7",
			"Custom8",
			"Custom9"
		};
		int selectedCollisionMask = 1;

		CollisionMask collisionMask = rigidBody->GetCollisionMask();
		if (collisionMask == CollisionMask::Default) selectedCollisionMask = 1;
		else if (collisionMask == CollisionMask::BlockWorldDynamic) selectedCollisionMask = 2;
		else if (collisionMask == CollisionMask::BlockWorldStatic) selectedCollisionMask = 3;
		else if (collisionMask == CollisionMask::BlockCharacter) selectedCollisionMask = 4;
		else if (collisionMask == CollisionMask::BlockAll) selectedCollisionMask = 5;
		else if (collisionMask == CollisionMask::BlockAllExceptCharacter) selectedCollisionMask = 6;
		else if (collisionMask == CollisionMask::OverlapWorldDynamic) selectedCollisionMask = 7;
		else if (collisionMask == CollisionMask::OverlapWorldStatic) selectedCollisionMask = 8;
		else if (collisionMask == CollisionMask::OverlapCharacter) selectedCollisionMask = 9;
		else if (collisionMask == CollisionMask::OverlapAll) selectedCollisionMask = 10;
		else if (collisionMask == CollisionMask::OverlapAllExceptCharacter) selectedCollisionMask = 11;
		else if (collisionMask == CollisionMask::BlockAndOverlapAll) selectedCollisionMask = 12;
		else if (collisionMask == CollisionMask::Custom0) selectedCollisionMask = 13;
		else if (collisionMask == CollisionMask::Custom1) selectedCollisionMask = 14;
		else if (collisionMask == CollisionMask::Custom2) selectedCollisionMask = 15;
		else if (collisionMask == CollisionMask::Custom3) selectedCollisionMask = 16;
		else if (collisionMask == CollisionMask::Custom4) selectedCollisionMask = 17;
		else if (collisionMask == CollisionMask::Custom5) selectedCollisionMask = 18;
		else if (collisionMask == CollisionMask::Custom6) selectedCollisionMask = 19;
		else if (collisionMask == CollisionMask::Custom7) selectedCollisionMask = 20;
		else if (collisionMask == CollisionMask::Custom8) selectedCollisionMask = 21;
		else if (collisionMask == CollisionMask::Custom9) selectedCollisionMask = 22;

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, 125);
		ImGui::Text("CollisionMask: ");
		ImGui::NextColumn();
		ImGui::SetColumnWidth(1, 800);
		bool collisionMaskCheck = ImGui::Combo("##CollisionMask", &selectedCollisionMask, collisionMaskNames, IM_ARRAYSIZE(collisionMaskNames));
		if (collisionMaskCheck)
		{
			CollisionMask newCollisionMask;
			if (selectedCollisionMask == 2)
			{
				newCollisionMask = CollisionMask::BlockWorldDynamic;
			}
			else if (selectedCollisionMask == 3)
			{
				newCollisionMask = CollisionMask::BlockWorldStatic;
			}
			else if (selectedCollisionMask == 4)
			{
				newCollisionMask = CollisionMask::BlockCharacter;
			}
			else if (selectedCollisionMask == 5)
			{
				newCollisionMask = CollisionMask::BlockAll;
			}
			else if (selectedCollisionMask == 6)
			{
				newCollisionMask = CollisionMask::BlockAllExceptCharacter;
			}
			else if (selectedCollisionMask == 7)
			{
				newCollisionMask = CollisionMask::OverlapWorldDynamic;
			}
			else if (selectedCollisionMask == 8)
			{
				newCollisionMask = CollisionMask::OverlapWorldStatic;
			}
			else if (selectedCollisionMask == 9)
			{
				newCollisionMask = CollisionMask::OverlapCharacter;
			}
			else if (selectedCollisionMask == 10)
			{
				newCollisionMask = CollisionMask::OverlapAll;
			}
			else if (selectedCollisionMask == 11)
			{
				newCollisionMask = CollisionMask::OverlapAllExceptCharacter;
			}
			else if (selectedCollisionMask == 12)
			{
				newCollisionMask = CollisionMask::BlockAndOverlapAll;
			}
			else if (selectedCollisionMask == 13)
			{
				newCollisionMask = CollisionMask::Custom0;
			}
			else if (selectedCollisionMask == 14)
			{
				newCollisionMask = CollisionMask::Custom1;
			}
			else if (selectedCollisionMask == 15)
			{
				newCollisionMask = CollisionMask::Custom2;
			}
			else if (selectedCollisionMask == 16)
			{
				newCollisionMask = CollisionMask::Custom3;
			}
			else if (selectedCollisionMask == 17)
			{
				newCollisionMask = CollisionMask::Custom4;
			}
			else if (selectedCollisionMask == 18)
			{
				newCollisionMask = CollisionMask::Custom5;
			}
			else if (selectedCollisionMask == 19)
			{
				newCollisionMask = CollisionMask::Custom6;
			}
			else if (selectedCollisionMask == 20)
			{
				newCollisionMask = CollisionMask::Custom7;
			}
			else if (selectedCollisionMask == 21)
			{
				newCollisionMask = CollisionMask::Custom8;
			}
			else if (selectedCollisionMask == 22)
			{
				newCollisionMask = CollisionMask::Custom9;
			}
			else
			{
				newCollisionMask = CollisionMask::Default;
			}
			rigidBody->SetCollisionMask(newCollisionMask);
			MarkSceneDirty("Rigid body collision mask changed");
		}
		ImGui::Columns(1, nullptr, false);
	}
	ImGui::Separator();

	const std::vector<Component*>& components = object->GetComponents();
	for (Component* component : components)
	{
		DrawComponentDetails(object, component);
		ImGui::Separator();
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawMultipleObjectDetails()
{
	const std::vector<ObjectBase*>& selectedObjects = EditorContext::Get()->GetSelectedObjects();
	std::vector<ObjectBase*> objects;
	for (ObjectBase* selectedObject : selectedObjects)
	{
		if (selectedObject)
		{
			objects.push_back(selectedObject);
		}
	}

	if (objects.size() <= 1)
	{
		DrawObjectDetails();
		return;
	}

	auto setBatchAssetSelection = [this](const std::vector<void*>& components, DetailsAssetSelectionTarget target, EditorAssetType filter)
		{
			if (components.empty())
			{
				return;
			}

			assetSelectionComponents_ = components;
			assetSelectionComponent_ = components.front();
			assetSelectionComponentType_ = target;
			assetSelectionSubMeshIndex_ = -1;
			EditorContext::Get()->assetSelectorFilter = filter;

			AssetSelectorPanel::OnAssetSelected =
				Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

			hud_->ShowPanel<AssetSelectorPanel>();
		};

	ImGui::Text("%d Objects Selected", static_cast<int>(objects.size()));
	ImGui::Separator();

	const MultiObjectDetailsClass objectClass = GetCommonObjectDetailsClass(objects);
	ImGui::Text("Class: %s", GetObjectDetailsClassName(objectClass));

	ImGui::PushItemWidth(50.f);

	if (objectClass != MultiObjectDetailsClass::Mixed)
	{
		if (DrawBatchStringField(
			"Name",
			"##MultiObjectName",
			objects,
			[](ObjectBase* object) { return object->GetNameWithoutId(); },
			[](ObjectBase* object, const std::string& value) { object->SetName(value); }))
		{
			MarkSceneDirty("Object name changed");
		}

		bool objectTransformChanged = false;
		objectTransformChanged |= DrawBatchVector3Field(
			"Position",
			"##MultiObjectPosition",
			objects,
			[](ObjectBase* object) { return object->GetWorldPosition(); },
			[](ObjectBase* object, const Vector3& value) { object->SetWorldPosition(value); });

		objectTransformChanged |= DrawBatchVector3Field(
			"Rotation",
			"##MultiObjectRotation",
			objects,
			[](ObjectBase* object) { return object->GetWorldRotation().ToEulerDegrees(); },
			[](ObjectBase* object, const Vector3& value) { object->SetWorldRotation(Quaternion::FromEulerDegrees(value)); });

		objectTransformChanged |= DrawBatchVector3Field(
			"Scaling",
			"##MultiObjectScaling",
			objects,
			[](ObjectBase* object) { return object->GetWorldScaling(); },
			[](ObjectBase* object, const Vector3& value) { object->SetWorldScaling(value); });

		if (objectTransformChanged)
		{
			for (ObjectBase* object : objects)
			{
				if (ObjectHasNavigationTreeComponent(object))
				{
					RebuildSceneNavigationMesh();
					break;
				}
			}

			MarkSceneDirty("Object transform changed");
		}

		if (DrawBatchBoolField(
			"Active",
			"##MultiObjectActive",
			objects,
			[](ObjectBase* object) { return object->GetIsActive(); },
			[](ObjectBase* object, bool value) { object->SetIsActive(value); }))
		{
			MarkSceneDirty("Object active state changed");
		}

		if (DrawBatchBoolField(
			"Tickable",
			"##MultiObjectTickable",
			objects,
			[](ObjectBase* object) { return object->GetIsTickable(); },
			[](ObjectBase* object, bool value) { object->SetIsTickable(value); }))
		{
			MarkSceneDirty("Object tickable state changed");
		}

		if (DrawBatchBoolField(
			"Tick Enabled",
			"##MultiObjectTickEnabled",
			objects,
			[](ObjectBase* object) { return object->GetIsTickEnabled(); },
			[](ObjectBase* object, bool value) { object->SetIsTickEnabled(value); }))
		{
			MarkSceneDirty("Object tick state changed");
		}

		if (objectClass == MultiObjectDetailsClass::PhysicsObject ||
			objectClass == MultiObjectDetailsClass::RigidBody ||
			objectClass == MultiObjectDetailsClass::OverlappingPhysicsObject)
		{
			const std::vector<PhysicsObject*> physicsObjects = CastItems<PhysicsObject>(objects);
			ImGui::Separator();
			ImGui::Text("PhysicsObject");

			if (DrawBatchStringField(
				"Tag",
				"##MultiPhysicsObjectTag",
				physicsObjects,
				[](PhysicsObject* physicsObject) { return physicsObject->GetTag(); },
				[](PhysicsObject* physicsObject, const std::string& value) { physicsObject->SetTag(value); }))
			{
				MarkSceneDirty("Physics object tag changed");
			}

			if (DrawBatchBoolField(
				"Physics Tick Enabled",
				"##MultiPhysicsTickEnabled",
				physicsObjects,
				[](PhysicsObject* physicsObject) { return physicsObject->GetPhysicsTickEnabled(); },
				[](PhysicsObject* physicsObject, bool value) { physicsObject->SetPhysicsTickEnabled(value); }))
			{
				MarkSceneDirty("Physics tick state changed");
			}

			if (DrawBatchEnumComboField(
				"CollisionGroup",
				"##MultiPhysicsObjectCollisionGroup",
				physicsObjects,
				[](PhysicsObject* physicsObject) { return physicsObject->GetCollisionGroup(); },
				[](PhysicsObject* physicsObject, CollisionGroup value) { physicsObject->SetCollisionGroup(value); },
				CollisionGroupValues))
			{
				MarkSceneDirty("Physics object collision group changed");
			}

			if (DrawBatchEnumComboField(
				"CollisionMask",
				"##MultiPhysicsObjectCollisionMask",
				physicsObjects,
				[](PhysicsObject* physicsObject) { return physicsObject->GetCollisionMask(); },
				[](PhysicsObject* physicsObject, CollisionMask value) { physicsObject->SetCollisionMask(value); },
				CollisionMaskValues))
			{
				MarkSceneDirty("Physics object collision mask changed");
			}

			if (DrawBatchEnumComboField(
				"CollisionFlag",
				"##MultiPhysicsObjectCollisionFlag",
				physicsObjects,
				[](PhysicsObject* physicsObject) { return physicsObject->GetCollisionFlag(); },
				[](PhysicsObject* physicsObject, CollisionFlag value) { physicsObject->SetCollisionFlag(value); },
				CollisionFlagValues))
			{
				MarkSceneDirty("Physics object collision flag changed");
			}
		}

		if (objectClass == MultiObjectDetailsClass::RigidBody)
		{
			const std::vector<RigidBody*> rigidBodies = CastItems<RigidBody>(objects);
			ImGui::Separator();
			ImGui::Text("Rigidbody");

			if (DrawBatchFloatField(
				"Mass",
				"##MultiRigidBodyMass",
				rigidBodies,
				[](RigidBody* rigidBody) { return rigidBody->GetMass(); },
				[](RigidBody* rigidBody, float value) { rigidBody->SetMass(value); }))
			{
				MarkSceneDirty("Rigid body mass changed");
			}
		}
	}

	ImGui::Separator();
	ImGui::Text("Mutual Components");

	const std::vector<MultiComponentDetailsGroup> componentGroups = GetMutualComponentDetailsGroups(objects);
	if (componentGroups.empty())
	{
		ImGui::TextDisabled("No mutual components");
		ImGui::PopItemWidth();
		return;
	}

	for (size_t componentGroupIndex = 0; componentGroupIndex < componentGroups.size(); ++componentGroupIndex)
	{
		const MultiComponentDetailsGroup& componentGroup = componentGroups[componentGroupIndex];
		ImGui::PushID(static_cast<int>(componentGroupIndex));

		std::string componentTitle = componentGroup.typeName;
		if (componentGroup.occurrenceIndex > 0)
		{
			componentTitle += " " + std::to_string(componentGroup.occurrenceIndex + 1);
		}

		bool firstIsRootComponent = false;
		bool hasRootComponentDiff = false;
		bool hasConstructorOwnedComponent = false;
		bool allComponentsAreConstructorOwned = !componentGroup.components.empty();
		if (!componentGroup.components.empty())
		{
			Component* firstComponent = componentGroup.components.front();
			firstIsRootComponent = firstComponent && firstComponent->GetOwner() && firstComponent->GetOwner()->GetRootComponent() == firstComponent;
			for (Component* component : componentGroup.components)
			{
				const bool isRootComponent = component && component->GetOwner() && component->GetOwner()->GetRootComponent() == component;
				if (isRootComponent != firstIsRootComponent)
				{
					hasRootComponentDiff = true;
				}

				const bool isConstructorOwnedComponent = EditorRuntimeDynamicObjectFactoryRegistrar::IsConstructorOwnedComponent(component);
				hasConstructorOwnedComponent |= isConstructorOwnedComponent;
				allComponentsAreConstructorOwned &= isConstructorOwnedComponent;
			}
		}

		if (hasRootComponentDiff)
		{
			componentTitle += " (Root Diff)";
		}
		else if (firstIsRootComponent)
		{
			componentTitle += " (RootComponent)";
		}
		if (allComponentsAreConstructorOwned)
		{
			componentTitle += " (Inherited)";
		}
		else if (hasConstructorOwnedComponent)
		{
			componentTitle += " (Mixed Inherited)";
		}

		ImGui::Separator();
		ImGui::Text("%s", componentTitle.c_str());
		if (hasConstructorOwnedComponent)
		{
			ImGui::TextDisabled("Defined by class constructor");
		}
		ImGui::BeginDisabled(hasConstructorOwnedComponent);

		bool componentTransformChanged = false;
		componentTransformChanged |= DrawBatchVector3Field(
			"RelativePosition",
			"##MultiComponentRelativePosition",
			componentGroup.components,
			[](Component* component) { return component->GetRelativePosition(); },
			[](Component* component, const Vector3& value) { component->SetRelativePosition(value); });

		componentTransformChanged |= DrawBatchVector3Field(
			"RelativeRotation",
			"##MultiComponentRelativeRotation",
			componentGroup.components,
			[](Component* component) { return component->GetRelativeRotation().ToEulerDegrees(); },
			[](Component* component, const Vector3& value) { component->SetRelativeRotation(Quaternion::FromEulerDegrees(value)); });

		componentTransformChanged |= DrawBatchVector3Field(
			"RelativeScaling",
			"##MultiComponentRelativeScaling",
			componentGroup.components,
			[](Component* component) { return component->GetRelativeScaling(); },
			[](Component* component, const Vector3& value) { component->SetRelativeScaling(value); });

		if (componentTransformChanged)
		{
			for (Component* component : componentGroup.components)
			{
				if (dynamic_cast<NavigationTreeComponent*>(component))
				{
					RebuildSceneNavigationMesh();
					break;
				}
			}

			MarkSceneDirty("Component transform changed");
		}

		if (DrawBatchBoolField(
			"Active",
			"##MultiComponentActive",
			componentGroup.components,
			[](Component* component) { return component->GetIsActive(); },
			[](Component* component, bool value) { component->SetIsActive(value); }))
		{
			MarkSceneDirty("Component active state changed");
		}

		if (DrawBatchBoolField(
			"Tick Enabled",
			"##MultiComponentTickEnabled",
			componentGroup.components,
			[](Component* component) { return component->GetIsTickEnabled(); },
			[](Component* component, bool value) { component->SetIsTickEnabled(value); }))
		{
			MarkSceneDirty("Component tick state changed");
		}

		const std::vector<CollisionComponent*> collisionComponents = CastItems<CollisionComponent>(componentGroup.components);
		if (!collisionComponents.empty())
		{
			if (DrawBatchEnumComboField(
				"CollisionGroup",
				"##MultiCollisionComponentCollisionGroup",
				collisionComponents,
				[](CollisionComponent* collisionComponent) { return collisionComponent->GetCollisionGroup(); },
				[](CollisionComponent* collisionComponent, CollisionGroup value) { collisionComponent->SetCollisionGroup(value); },
				CollisionGroupValues))
			{
				MarkSceneDirty("Collision component collision group changed");
			}

			if (DrawBatchEnumComboField(
				"CollisionMask",
				"##MultiCollisionComponentCollisionMask",
				collisionComponents,
				[](CollisionComponent* collisionComponent) { return collisionComponent->GetCollisionMask(); },
				[](CollisionComponent* collisionComponent, CollisionMask value) { collisionComponent->SetCollisionMask(value); },
				CollisionMaskValues))
			{
				MarkSceneDirty("Collision component collision mask changed");
			}
		}

		const std::vector<StaticMeshComponent*> staticMeshComponents = CastItems<StaticMeshComponent>(componentGroup.components);
		if (!staticMeshComponents.empty())
		{
			DrawBatchTextValue(
				"Mesh",
				staticMeshComponents,
				[](StaticMeshComponent* staticMeshComponent)
				{
					StaticMeshInstance* meshInstance = staticMeshComponent->GetMeshInstance();
					StaticMesh* mesh = meshInstance ? meshInstance->GetMesh() : nullptr;
					return mesh ? ToDisplayContentPath(mesh->GetPath()) : std::string();
				});

			if (ImGui::Button("Select asset##MultiStaticMeshAsset"))
			{
				std::vector<void*> assetSelectionComponents;
				for (StaticMeshComponent* staticMeshComponent : staticMeshComponents)
				{
					assetSelectionComponents.push_back(staticMeshComponent);
				}
				setBatchAssetSelection(assetSelectionComponents, DetailsAssetSelectionTarget::StaticMesh, EditorAssetType::StaticMesh);
			}

			if (DrawBatchIntField(
				"Render mask",
				"##MultiStaticMeshRenderMask",
				staticMeshComponents,
				[](StaticMeshComponent* staticMeshComponent)
				{
					StaticMeshInstance* meshInstance = staticMeshComponent->GetMeshInstance();
					return meshInstance ? static_cast<int>(meshInstance->GetRenderMask()) : 0;
				},
				[](StaticMeshComponent* staticMeshComponent, int value)
				{
					StaticMeshInstance* meshInstance = staticMeshComponent->GetMeshInstance();
					if (meshInstance)
					{
						meshInstance->SetRenderMask(value);
					}
				}))
			{
				MarkSceneDirty("Static mesh render mask changed");
			}
		}

		const std::vector<SkeletalMeshComponent*> skeletalMeshComponents = CastItems<SkeletalMeshComponent>(componentGroup.components);
		if (!skeletalMeshComponents.empty())
		{
			DrawBatchTextValue(
				"Mesh",
				skeletalMeshComponents,
				[](SkeletalMeshComponent* skeletalMeshComponent)
				{
					SkeletalMeshInstance* meshInstance = skeletalMeshComponent->GetMeshInstance();
					SkeletalMesh* mesh = meshInstance ? meshInstance->GetMesh() : nullptr;
					return mesh ? ToDisplayContentPath(mesh->GetPath()) : std::string();
				});

			if (ImGui::Button("Select asset##MultiSkeletalMeshAsset"))
			{
				std::vector<void*> assetSelectionComponents;
				for (SkeletalMeshComponent* skeletalMeshComponent : skeletalMeshComponents)
				{
					assetSelectionComponents.push_back(skeletalMeshComponent);
				}
				setBatchAssetSelection(assetSelectionComponents, DetailsAssetSelectionTarget::SkeletalMesh, EditorAssetType::SkeletalMesh);
			}

			if (DrawBatchIntField(
				"Render mask",
				"##MultiSkeletalMeshRenderMask",
				skeletalMeshComponents,
				[](SkeletalMeshComponent* skeletalMeshComponent)
				{
					SkeletalMeshInstance* meshInstance = skeletalMeshComponent->GetMeshInstance();
					return meshInstance ? static_cast<int>(meshInstance->GetRenderMask()) : 0;
				},
				[](SkeletalMeshComponent* skeletalMeshComponent, int value)
				{
					SkeletalMeshInstance* meshInstance = skeletalMeshComponent->GetMeshInstance();
					if (meshInstance)
					{
						meshInstance->SetRenderMask(value);
					}
				}))
			{
				MarkSceneDirty("Skeletal mesh render mask changed");
			}
		}

		const std::vector<ParticleSystemComponent*> particleSystemComponents = CastItems<ParticleSystemComponent>(componentGroup.components);
		if (!particleSystemComponents.empty())
		{
			if (DrawBatchFloatField(
				"Particle Size",
				"##MultiParticleSystemParticleSize",
				particleSystemComponents,
				[](ParticleSystemComponent* particleSystemComponent) { return particleSystemComponent->GetParticleSize(); },
				[](ParticleSystemComponent* particleSystemComponent, float value) { particleSystemComponent->SetParticleSize(value); }))
			{
				MarkSceneDirty("Particle size changed");
			}

			if (DrawBatchVector3Field(
				"Gravity",
				"##MultiParticleSystemGravity",
				particleSystemComponents,
				[](ParticleSystemComponent* particleSystemComponent) { return particleSystemComponent->GetGravity(); },
				[](ParticleSystemComponent* particleSystemComponent, const Vector3& value) { particleSystemComponent->SetGravity(value); }))
			{
				MarkSceneDirty("Particle gravity changed");
			}

			if (DrawBatchIntField(
				"Max Particle Count",
				"##MultiParticleSystemMaxParticleCount",
				particleSystemComponents,
				[](ParticleSystemComponent* particleSystemComponent) { return static_cast<int>(particleSystemComponent->GetMaxParticleCount()); },
				[](ParticleSystemComponent* particleSystemComponent, int value) { particleSystemComponent->SetMaxParticleCount(static_cast<std::uint32_t>((std::max)(value, 0))); }))
			{
				MarkSceneDirty("Particle max count changed");
			}

			if (DrawBatchIntField(
				"Preview Burst Count",
				"##MultiParticleSystemPreviewCount",
				particleSystemComponents,
				[](ParticleSystemComponent* particleSystemComponent) { return static_cast<int>(particleSystemComponent->GetPreviewParticleCount()); },
				[](ParticleSystemComponent* particleSystemComponent, int value) { particleSystemComponent->SetPreviewParticleCount(static_cast<std::uint32_t>((std::max)(value, 0))); }))
			{
				MarkSceneDirty("Particle preview count changed");
			}
		}

		const std::vector<DirectionalLightComponent*> directionalLightComponents = CastItems<DirectionalLightComponent>(componentGroup.components);
		if (!directionalLightComponents.empty())
		{
			if (DrawBatchLightComponentCommonFields("DirectionalLightComponent", directionalLightComponents))
			{
				MarkSceneDirty("Directional light component changed");
			}
		}

		const std::vector<PointLightComponent*> pointLightComponents = CastItems<PointLightComponent>(componentGroup.components);
		if (!pointLightComponents.empty())
		{
			bool changed = DrawBatchLightComponentCommonFields("PointLightComponent", pointLightComponents);
			changed |= DrawBatchFloatField(
				"Radius",
				"##MultiPointLightComponentRadius",
				pointLightComponents,
				[](PointLightComponent* component)
				{
					const PointLight* light = component ? component->GetLight() : nullptr;
					return light ? light->GetRadius() : 0.f;
				},
				[](PointLightComponent* component, float value)
				{
					if (PointLight* light = component ? component->GetLight() : nullptr)
					{
						light->SetRadius(value);
					}
				});

			if (changed)
			{
				MarkSceneDirty("Point light component changed");
			}
		}

		const std::vector<SpotLightComponent*> spotLightComponents = CastItems<SpotLightComponent>(componentGroup.components);
		if (!spotLightComponents.empty())
		{
			bool changed = DrawBatchLightComponentCommonFields("SpotLightComponent", spotLightComponents);
			changed |= DrawBatchFloatField(
				"FalloffAngle",
				"##MultiSpotLightComponentFalloffAngle",
				spotLightComponents,
				[](SpotLightComponent* component)
				{
					const SpotLight* light = component ? component->GetLight() : nullptr;
					return light ? RADIAN_TO_DEGREE(light->GetFalloffAngle()) : 0.f;
				},
				[](SpotLightComponent* component, float value)
				{
					if (SpotLight* light = component ? component->GetLight() : nullptr)
					{
						light->SetFalloffAngle(value);
					}
				});

			changed |= DrawBatchFloatField(
				"CoverageAngle",
				"##MultiSpotLightComponentCoverageAngle",
				spotLightComponents,
				[](SpotLightComponent* component)
				{
					const SpotLight* light = component ? component->GetLight() : nullptr;
					return light ? RADIAN_TO_DEGREE(light->GetCoverageAngle()) : 0.f;
				},
				[](SpotLightComponent* component, float value)
				{
					if (SpotLight* light = component ? component->GetLight() : nullptr)
					{
						light->SetCoverageAngle(value);
					}
				});

			changed |= DrawBatchFloatField(
				"Radius",
				"##MultiSpotLightComponentRadius",
				spotLightComponents,
				[](SpotLightComponent* component)
				{
					const SpotLight* light = component ? component->GetLight() : nullptr;
					return light ? light->GetRadius() : 0.f;
				},
				[](SpotLightComponent* component, float value)
				{
					if (SpotLight* light = component ? component->GetLight() : nullptr)
					{
						light->SetRadius(value);
					}
				});

			if (changed)
			{
				MarkSceneDirty("Spot light component changed");
			}
		}

		const std::vector<StaticMeshParticleSystemComponent*> staticMeshParticleSystemComponents = CastItems<StaticMeshParticleSystemComponent>(componentGroup.components);
		if (!staticMeshParticleSystemComponents.empty())
		{
			DrawBatchTextValue(
				"Static Mesh",
				staticMeshParticleSystemComponents,
				[](StaticMeshParticleSystemComponent* component) { return component->GetStaticMeshPath(); });
		}

		const std::vector<BillboardParticleSystemComponent*> billboardParticleSystemComponents = CastItems<BillboardParticleSystemComponent>(componentGroup.components);
		if (!billboardParticleSystemComponents.empty())
		{
			DrawBatchTextValue(
				"Billboard Material",
				billboardParticleSystemComponents,
				[](BillboardParticleSystemComponent* component) { return component->GetBillboardMaterialPath(); });
			DrawBatchTextValue(
				"Billboard Texture",
				billboardParticleSystemComponents,
				[](BillboardParticleSystemComponent* component) { return component->GetBillboardTexturePath(); });
		}

		const std::vector<BoxCollisionComponent*> boxCollisionComponents = CastItems<BoxCollisionComponent>(componentGroup.components);
		if (!boxCollisionComponents.empty())
		{
			if (DrawBatchVector3Field(
				"HalfSize",
				"##MultiBoxCollisionHalfSize",
				boxCollisionComponents,
				[](BoxCollisionComponent* boxCollisionComponent) { return boxCollisionComponent->GetHalfSize(); },
				[](BoxCollisionComponent* boxCollisionComponent, const Vector3& value) { boxCollisionComponent->SetHalfSize(value); }))
			{
				MarkSceneDirty("Box collision half size changed");
			}
		}

		const std::vector<SphereCollisionComponent*> sphereCollisionComponents = CastItems<SphereCollisionComponent>(componentGroup.components);
		if (!sphereCollisionComponents.empty())
		{
			if (DrawBatchFloatField(
				"Radius",
				"##MultiSphereCollisionRadius",
				sphereCollisionComponents,
				[](SphereCollisionComponent* sphereCollisionComponent) { return sphereCollisionComponent->GetRadius(); },
				[](SphereCollisionComponent* sphereCollisionComponent, float value) { sphereCollisionComponent->SetRadius(value); }))
			{
				MarkSceneDirty("Sphere collision radius changed");
			}
		}

		const std::vector<CapsuleCollisionComponent*> capsuleCollisionComponents = CastItems<CapsuleCollisionComponent>(componentGroup.components);
		if (!capsuleCollisionComponents.empty())
		{
			if (DrawBatchFloatField(
				"Radius",
				"##MultiCapsuleCollisionRadius",
				capsuleCollisionComponents,
				[](CapsuleCollisionComponent* capsuleCollisionComponent) { return capsuleCollisionComponent->GetRadius(); },
				[](CapsuleCollisionComponent* capsuleCollisionComponent, float value) { capsuleCollisionComponent->SetRadius(value); }))
			{
				MarkSceneDirty("Capsule collision radius changed");
			}

			if (DrawBatchFloatField(
				"Height",
				"##MultiCapsuleCollisionHeight",
				capsuleCollisionComponents,
				[](CapsuleCollisionComponent* capsuleCollisionComponent) { return capsuleCollisionComponent->GetHeight(); },
				[](CapsuleCollisionComponent* capsuleCollisionComponent, float value) { capsuleCollisionComponent->SetHeight(value); }))
			{
				MarkSceneDirty("Capsule collision height changed");
			}
		}

		const std::vector<HeightMapCollisionComponent*> heightMapCollisionComponents = CastItems<HeightMapCollisionComponent>(componentGroup.components);
		if (!heightMapCollisionComponents.empty())
		{
			DrawBatchTextValue(
				"HeightMapImage",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component)
				{
					const Image* image = component->GetHeightMapImage();
					return image ? ToDisplayContentPath(image->GetPath()) : std::string();
				});

			if (ImGui::Button("Select asset##MultiHeightMapImageAsset"))
			{
				std::vector<void*> assetSelectionComponents;
				for (HeightMapCollisionComponent* component : heightMapCollisionComponents)
				{
					assetSelectionComponents.push_back(component);
				}
				setBatchAssetSelection(assetSelectionComponents, DetailsAssetSelectionTarget::HeightMapImage, EditorAssetType::Texture);
			}

			bool hasHeightMapImage = false;
			for (HeightMapCollisionComponent* component : heightMapCollisionComponents)
			{
				if (component->GetHeightMapImage())
				{
					hasHeightMapImage = true;
					break;
				}
			}

			if (hasHeightMapImage)
			{
				ImGui::SameLine();
				if (ImGui::Button("Clear##MultiHeightMapImageAsset"))
				{
					for (HeightMapCollisionComponent* component : heightMapCollisionComponents)
					{
						component->SetHeightMapImage(nullptr);
					}
					MarkSceneDirty("Height map image cleared");
				}
			}

			if (DrawBatchIntField(
				"HeightStickWidth",
				"##MultiHeightMapCollisionHeightStickWidth",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetHeightStickWidth(); },
				[](HeightMapCollisionComponent* component, int value) { component->SetHeightStickWidth((std::max)(value, 2)); }))
			{
				MarkSceneDirty("Height map stick width changed");
			}

			if (DrawBatchIntField(
				"HeightStickLength",
				"##MultiHeightMapCollisionHeightStickLength",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetHeightStickLength(); },
				[](HeightMapCollisionComponent* component, int value) { component->SetHeightStickLength((std::max)(value, 2)); }))
			{
				MarkSceneDirty("Height map stick length changed");
			}

			if (DrawBatchFloatField(
				"MinHeight",
				"##MultiHeightMapCollisionMinHeight",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetMinHeight(); },
				[](HeightMapCollisionComponent* component, float value) { component->SetMinHeight(value); }))
			{
				MarkSceneDirty("Height map min height changed");
			}

			if (DrawBatchFloatField(
				"MaxHeight",
				"##MultiHeightMapCollisionMaxHeight",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetMaxHeight(); },
				[](HeightMapCollisionComponent* component, float value) { component->SetMaxHeight(value); }))
			{
				MarkSceneDirty("Height map max height changed");
			}

			if (DrawBatchFloatField(
				"Width",
				"##MultiHeightMapCollisionWidth",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetWidth(); },
				[](HeightMapCollisionComponent* component, float value) { component->SetWidth(value); }))
			{
				MarkSceneDirty("Height map width changed");
			}

			if (DrawBatchFloatField(
				"Length",
				"##MultiHeightMapCollisionLength",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetLength(); },
				[](HeightMapCollisionComponent* component, float value) { component->SetLength(value); }))
			{
				MarkSceneDirty("Height map length changed");
			}

			if (DrawBatchFloatField(
				"HeightScale",
				"##MultiHeightMapCollisionHeightScale",
				heightMapCollisionComponents,
				[](HeightMapCollisionComponent* component) { return component->GetHeightScale(); },
				[](HeightMapCollisionComponent* component, float value) { component->SetHeightScale(value); }))
			{
				MarkSceneDirty("Height map scale changed");
			}
		}

		const std::vector<MovingTriangleMeshCollisionComponent*> movingTriangleMeshCollisionComponents = CastItems<MovingTriangleMeshCollisionComponent>(componentGroup.components);
		if (!movingTriangleMeshCollisionComponents.empty())
		{
			DrawBatchTextValue(
				"Mesh",
				movingTriangleMeshCollisionComponents,
				[](MovingTriangleMeshCollisionComponent* component)
				{
					const StaticMesh* mesh = component->GetMesh();
					return mesh ? ToDisplayContentPath(mesh->GetPath()) : std::string();
				});
		}

		const std::vector<NonMovingTriangleMeshCollisionComponent*> nonMovingTriangleMeshCollisionComponents = CastItems<NonMovingTriangleMeshCollisionComponent>(componentGroup.components);
		if (!nonMovingTriangleMeshCollisionComponents.empty())
		{
			DrawBatchTextValue(
				"Mesh",
				nonMovingTriangleMeshCollisionComponents,
				[](NonMovingTriangleMeshCollisionComponent* component)
				{
					const StaticMesh* mesh = component->GetMesh();
					return mesh ? ToDisplayContentPath(mesh->GetPath()) : std::string();
				});
		}

		const std::vector<NavigationTreeComponent*> navigationTreeComponents = CastItems<NavigationTreeComponent>(componentGroup.components);
		if (!navigationTreeComponents.empty())
		{
			DrawBatchTextValue(
				"Navigation Tree",
				navigationTreeComponents,
				[](NavigationTreeComponent* navigationTreeComponent) { return navigationTreeComponent->GetNavigationTreePath(); });

			if (ImGui::Button("Select asset##MultiNavigationTreeAsset"))
			{
				std::vector<void*> assetSelectionComponents;
				for (NavigationTreeComponent* navigationTreeComponent : navigationTreeComponents)
				{
					assetSelectionComponents.push_back(navigationTreeComponent);
				}
				setBatchAssetSelection(assetSelectionComponents, DetailsAssetSelectionTarget::NavigationTree, EditorAssetType::NavigationTree);
			}

			bool hasNavigationTreePath = false;
			for (NavigationTreeComponent* navigationTreeComponent : navigationTreeComponents)
			{
				if (!navigationTreeComponent->GetNavigationTreePath().empty())
				{
					hasNavigationTreePath = true;
					break;
				}
			}

			if (hasNavigationTreePath)
			{
				ImGui::SameLine();
				if (ImGui::Button("Clear##MultiNavigationTreeAsset"))
				{
					for (NavigationTreeComponent* navigationTreeComponent : navigationTreeComponents)
					{
						navigationTreeComponent->SetNavigationTreePath("");
					}
					RebuildSceneNavigationMesh();
					MarkSceneDirty("Navigation tree cleared");
				}
			}
		}

		ImGui::EndDisabled();
		ImGui::PopID();
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawReflectionProbeObjectDetails(ReflectionProbeObject* reflectionProbeObject)
{
	if (!reflectionProbeObject)
	{
		return;
	}

	Vector3 probeSize = reflectionProbeObject->GetSize();
	const std::string sizeFieldName = "##ReflectionProbeSize_" + std::to_string(reflectionProbeObject->GetGUID());

	ImGui::Text("Probe Size: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputVector3(sizeFieldName, probeSize))
	{
		reflectionProbeObject->SetSize(probeSize);
		MarkSceneDirty("Reflection probe size changed");
	}

	float captureDistance = reflectionProbeObject->GetCaptureDistance();
	const std::string captureDistanceFieldName = "##ReflectionProbeCaptureDistance_" + std::to_string(reflectionProbeObject->GetGUID());

	ImGui::Text("Capture Distance: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat(captureDistanceFieldName.c_str(), captureDistance))
	{
		reflectionProbeObject->SetCaptureDistance(captureDistance);
		MarkSceneDirty("Reflection probe capture distance changed");
	}
}

void DetailsPanel::DrawComponentDetails(ObjectBase*, Component* component)
{
	if (!component)
	{
		return;
	}

	ParticleSystemComponent* particleSystemComponent{ nullptr };
	DirectionalLightComponent* directionalLightComponent{ nullptr };
	PointLightComponent* pointLightComponent{ nullptr };
	SpotLightComponent* spotLightComponent{ nullptr };
	StaticMeshComponent* staticMeshComponent{ nullptr };
	SkeletalMeshComponent* skeletalMeshComponent{ nullptr };
	BoxCollisionComponent* boxCollisionComponent{ nullptr };
	SphereCollisionComponent* sphereCollisionComponent{ nullptr };
	CapsuleCollisionComponent* capsuleCollisionComponent{ nullptr };
	HeightMapCollisionComponent* heightMapCollisionComponent{ nullptr };
	MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent{ nullptr };
	NonMovingTriangleMeshCollisionComponent* nonMvingTriangleMeshCollisionComponent{ nullptr };
	NavigationTreeComponent* navigationTreeComponent{ nullptr };

	std::string componentTypeString;

	particleSystemComponent = dynamic_cast<ParticleSystemComponent*>(component);
	directionalLightComponent = dynamic_cast<DirectionalLightComponent*>(component);
	pointLightComponent = dynamic_cast<PointLightComponent*>(component);
	spotLightComponent = dynamic_cast<SpotLightComponent*>(component);
	staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component);
	skeletalMeshComponent = dynamic_cast<SkeletalMeshComponent*>(component);
	boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component);
	sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component);
	capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component);
	heightMapCollisionComponent = dynamic_cast<HeightMapCollisionComponent*>(component);
	movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component);
	nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component);
	navigationTreeComponent = dynamic_cast<NavigationTreeComponent*>(component);

	if (particleSystemComponent)
	{
		componentTypeString = dynamic_cast<StaticMeshParticleSystemComponent*>(component) ?
			"StaticMeshParticleSystemComponent" :
			"BillboardParticleSystemComponent";
	}
	else if (staticMeshComponent)
	{
		componentTypeString = "StaticMeshComponent";
	}
	else if (directionalLightComponent)
	{
		componentTypeString = "DirectionalLightComponent";
	}
	else if (pointLightComponent)
	{
		componentTypeString = "PointLightComponent";
	}
	else if (spotLightComponent)
	{
		componentTypeString = "SpotLightComponent";
	}
	else if (skeletalMeshComponent)
	{
		componentTypeString = "SkeletalMeshComponent";
	}
	else if (boxCollisionComponent)
	{
		componentTypeString = "BoxCollisionComponent";
	}
	else if (sphereCollisionComponent)
	{
		componentTypeString = "SphereCollisionComponent";
	}
	else if (capsuleCollisionComponent)
	{
		componentTypeString = "CapsuleCollisionComponent";
	}
	else if (heightMapCollisionComponent)
	{
		componentTypeString = "HeightMapCollisionComponent";
	}
	else if (movingTriangleMeshCollisionComponent)
	{
		componentTypeString = "MovingTriangleMeshCollisionComponent";
	}
	else if (nonMvingTriangleMeshCollisionComponent)
	{
		componentTypeString = "NonMovingTriangleMeshCollisionComponent";
	}
	else if (navigationTreeComponent)
	{
		componentTypeString = "NavigationTreeComponent";
	}
	else
	{
		componentTypeString = "Component";
	}

	const bool isConstructorOwnedComponent = EditorRuntimeDynamicObjectFactoryRegistrar::IsConstructorOwnedComponent(component);
	if (component->GetOwner()->GetRootComponent() == component)
	{
		componentTypeString += " (RootComponent)";
	}
	if (isConstructorOwnedComponent)
	{
		componentTypeString += " (Inherited)";
	}

	ImGui::Text(componentTypeString.c_str());
	if (isConstructorOwnedComponent)
	{
		ImGui::TextDisabled("Defined by class constructor");
	}

	ImGui::Separator();
	ImGui::BeginDisabled(isConstructorOwnedComponent);

	Vector3 componentRelativePosition = component->GetRelativePosition();
	Vector3 componentRelativeRotationEulerDegrees = component->GetRelativeRotation().ToEulerDegrees();
	Vector3 componentRelativeScaling = component->GetRelativeScaling();

	std::string specialPostfix = "##" + std::to_string(component->GetGUID());

	ImGui::Text("RelativePosition: ");
	ImGui::SameLine();
	const bool relativePositionChanged = EditorWidgets::DrawInputVector3(std::string("##RelativePosition") + specialPostfix, componentRelativePosition);
	component->SetRelativePosition(componentRelativePosition);

	ImGui::Text("RelativeRotation: ");
	ImGui::SameLine();
	const bool relativeRotationChanged = EditorWidgets::DrawInputVector3(std::string("##RelativeRotation") + specialPostfix, componentRelativeRotationEulerDegrees);
	component->SetRelativeRotation(Quaternion::FromEulerDegrees(componentRelativeRotationEulerDegrees));

	ImGui::Text("RelativeScaling: ");
	ImGui::SameLine();
	const bool relativeScalingChanged = EditorWidgets::DrawInputVector3(std::string("##RelativeScaling") + specialPostfix, componentRelativeScaling);
	component->SetRelativeScaling(componentRelativeScaling);

	if (relativePositionChanged || relativeRotationChanged || relativeScalingChanged)
	{
		if (navigationTreeComponent)
		{
			RebuildSceneNavigationMesh();
		}

		MarkSceneDirty("Component transform changed");
		RefreshDetailsCollisionDebug(component->GetOwner());
	}

	particleSystemComponent = dynamic_cast<ParticleSystemComponent*>(component);
	directionalLightComponent = dynamic_cast<DirectionalLightComponent*>(component);
	pointLightComponent = dynamic_cast<PointLightComponent*>(component);
	spotLightComponent = dynamic_cast<SpotLightComponent*>(component);
	staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component);
	skeletalMeshComponent = dynamic_cast<SkeletalMeshComponent*>(component);
	boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component);
	sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component);
	capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component);
	heightMapCollisionComponent = dynamic_cast<HeightMapCollisionComponent*>(component);
	movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component);
	nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component);
	navigationTreeComponent = dynamic_cast<NavigationTreeComponent*>(component);

	if (particleSystemComponent)
	{
		DrawParticleSystemComponentDetails(particleSystemComponent);
	}
	else if (staticMeshComponent)
	{
		DrawStaticMeshComponentDetails(staticMeshComponent);
	}
	else if (directionalLightComponent)
	{
		DrawDirectionalLightComponentDetails(directionalLightComponent);
	}
	else if (pointLightComponent)
	{
		DrawPointLightComponentDetails(pointLightComponent);
	}
	else if (spotLightComponent)
	{
		DrawSpotLightComponentDetails(spotLightComponent);
	}
	else if (skeletalMeshComponent)
	{
		DrawSkeletalMeshComponentDetails(skeletalMeshComponent);
	}
	else if (boxCollisionComponent)
	{
		DrawBoxCollisionComponentDetails(boxCollisionComponent);
	}
	else if (sphereCollisionComponent)
	{
		DrawSphereCollisionComponentDetails(sphereCollisionComponent);
	}
	else if (capsuleCollisionComponent)
	{
		DrawCapsuleCollisionComponentDetails(capsuleCollisionComponent);
	}
	else if (heightMapCollisionComponent)
	{
		DrawHeightMapCollisionComponentDetails(heightMapCollisionComponent);
	}
	else if (movingTriangleMeshCollisionComponent)
	{
		DrawMovingTriangleMeshCollisionComponentDetails(movingTriangleMeshCollisionComponent);
	}
	else if (nonMvingTriangleMeshCollisionComponent)
	{
		DrawNonMovingTriangleMeshCollisionComponentDetails(nonMvingTriangleMeshCollisionComponent);
	}
	else if (navigationTreeComponent)
	{
		DrawNavigationTreeComponentDetails(navigationTreeComponent);
	}

	ImGui::EndDisabled();
}

void DetailsPanel::DrawDirectionalLightComponentDetails(DirectionalLightComponent* directionalLightComponent)
{
	if (!directionalLightComponent)
	{
		return;
	}

	DirectionalLight* light = directionalLightComponent->GetLight();
	const std::string specialPostfix = "##" + std::to_string(directionalLightComponent->GetGUID());
	if (DrawLightComponentCommonDetails(light, specialPostfix))
	{
		MarkSceneDirty("Directional light component changed");
	}
}

void DetailsPanel::DrawPointLightComponentDetails(PointLightComponent* pointLightComponent)
{
	if (!pointLightComponent)
	{
		return;
	}

	PointLight* light = pointLightComponent->GetLight();
	const std::string specialPostfix = "##" + std::to_string(pointLightComponent->GetGUID());
	bool changed = DrawLightComponentCommonDetails(light, specialPostfix);

	if (light)
	{
		float radius = light->GetRadius();
		ImGui::Text("Radius: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##PointLightComponentRadius") + specialPostfix, radius))
		{
			light->SetRadius(radius);
			changed = true;
		}
	}

	if (changed)
	{
		MarkSceneDirty("Point light component changed");
	}
}

void DetailsPanel::DrawSpotLightComponentDetails(SpotLightComponent* spotLightComponent)
{
	if (!spotLightComponent)
	{
		return;
	}

	SpotLight* light = spotLightComponent->GetLight();
	const std::string specialPostfix = "##" + std::to_string(spotLightComponent->GetGUID());
	bool changed = DrawLightComponentCommonDetails(light, specialPostfix);

	if (light)
	{
		float falloffAngle = RADIAN_TO_DEGREE(light->GetFalloffAngle());
		ImGui::Text("FalloffAngle: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##SpotLightComponentFalloffAngle") + specialPostfix, falloffAngle))
		{
			light->SetFalloffAngle(falloffAngle);
			changed = true;
		}

		float coverageAngle = RADIAN_TO_DEGREE(light->GetCoverageAngle());
		ImGui::Text("CoverageAngle: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##SpotLightComponentCoverageAngle") + specialPostfix, coverageAngle))
		{
			light->SetCoverageAngle(coverageAngle);
			changed = true;
		}

		float radius = light->GetRadius();
		ImGui::Text("Radius: ");
		ImGui::SameLine();
		if (EditorWidgets::DrawInputFloat(std::string("##SpotLightComponentRadius") + specialPostfix, radius))
		{
			light->SetRadius(radius);
			changed = true;
		}
	}

	if (changed)
	{
		MarkSceneDirty("Spot light component changed");
	}
}

void DetailsPanel::DrawStaticMeshComponentDetails(StaticMeshComponent* staticMeshComponent)
{
	std::string specialPostfix = "##" + std::to_string(staticMeshComponent->GetGUID());

	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	StaticMesh* staticMesh = staticMeshComponent->GetMeshInstance()->GetMesh();
	ImGui::Text(staticMesh ? staticMesh->GetPath().substr(ContentDir.size()).c_str() : "");

	std::string specialName = std::string("Select asset") + specialPostfix;

	ImGui::SameLine();
	if (ImGui::Button(specialName.c_str()))
	{
		assetSelectionComponent_ = staticMeshComponent;
		assetSelectionComponentType_ = DetailsAssetSelectionTarget::StaticMesh;
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::StaticMesh;

		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

		hud_->ShowPanel<AssetSelectorPanel>();
	}

	ImGui::Text("Default Materials:");

	StaticMeshInstance* staticMeshInstance = staticMeshComponent->GetMeshInstance();
	const std::vector<std::string> materialPaths = SceneParser::GetStaticMeshComponentMaterialPaths(staticMeshComponent);
	const size_t subMeshCount = staticMesh ? staticMesh->GetSubMeshes().size() : 0;
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		ImGui::PushID(static_cast<int>(subMeshIndex));

		const std::string& subMeshName = staticMesh->GetSubMeshes()[subMeshIndex]->GetName();
		if (subMeshName.empty())
		{
			ImGui::Text("Sub Mesh %d", static_cast<int>(subMeshIndex));
		}
		else
		{
			ImGui::Text("Sub Mesh %d: %s", static_cast<int>(subMeshIndex), subMeshName.c_str());
		}

		const char* materialPath = subMeshIndex < materialPaths.size() && !materialPaths[subMeshIndex].empty() ? materialPaths[subMeshIndex].c_str() : "";
		ImGui::TextWrapped("%s", materialPath);

		if (ImGui::Button("Select Asset"))
		{
			assetSelectionComponent_ = staticMeshComponent;
			assetSelectionComponentType_ = DetailsAssetSelectionTarget::Material;
			assetSelectionSubMeshIndex_ = static_cast<int>(subMeshIndex);
			EditorContext::Get()->assetSelectorFilter = EditorAssetType::Material;

			AssetSelectorPanel::OnAssetSelected =
				Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

			hud_->ShowPanel<AssetSelectorPanel>();
		}

		ImGui::PopID();

		if (subMeshIndex + 1 < subMeshCount)
		{
			ImGui::Separator();
		}
	}

	ImGui::Text("Render mask: ");

	ImGui::SameLine();

	ImGui::PushItemWidth(100.f);

	int currentValue = staticMeshInstance->GetRenderMask();
	EditorWidgets::DrawInputInt("##StaticMeshInstanceRenderMask", currentValue);
	if (currentValue != (int)staticMeshInstance->GetRenderMask())
	{
		staticMeshInstance->SetRenderMask(currentValue);
		MarkSceneDirty("Static mesh render mask changed");
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawSkeletalMeshComponentDetails(SkeletalMeshComponent* skeletalMeshComponent)
{
	std::string specialPostfix = "##" + std::to_string(skeletalMeshComponent->GetGUID());

	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	SkeletalMeshInstance* skeletalMeshInstance = skeletalMeshComponent->GetMeshInstance();
	SkeletalMesh* skeletalMesh = skeletalMeshInstance ? skeletalMeshInstance->GetMesh() : nullptr;
	ImGui::Text(skeletalMesh ? skeletalMesh->GetPath().substr(ContentDir.size()).c_str() : "");

	std::string specialName = std::string("Select asset") + specialPostfix;

	ImGui::SameLine();
	if (ImGui::Button(specialName.c_str()))
	{
		assetSelectionComponent_ = skeletalMeshComponent;
		assetSelectionComponentType_ = DetailsAssetSelectionTarget::SkeletalMesh;
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::SkeletalMesh;

		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

		hud_->ShowPanel<AssetSelectorPanel>();
	}

	ImGui::Text("Default Materials:");

	const std::vector<std::string> materialPaths = SceneParser::GetSkeletalMeshComponentMaterialPaths(skeletalMeshComponent);
	const size_t subMeshCount = skeletalMesh ? skeletalMesh->GetSubMeshes().size() : 0;
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		ImGui::PushID(static_cast<int>(subMeshIndex));

		const std::string& subMeshName = skeletalMesh->GetSubMeshes()[subMeshIndex]->GetName();
		if (subMeshName.empty())
		{
			ImGui::Text("Sub Mesh %d", static_cast<int>(subMeshIndex));
		}
		else
		{
			ImGui::Text("Sub Mesh %d: %s", static_cast<int>(subMeshIndex), subMeshName.c_str());
		}

		const char* materialPath = subMeshIndex < materialPaths.size() && !materialPaths[subMeshIndex].empty() ? materialPaths[subMeshIndex].c_str() : "";
		ImGui::TextWrapped("%s", materialPath);

		if (ImGui::Button("Select Asset"))
		{
			assetSelectionComponent_ = skeletalMeshComponent;
			assetSelectionComponentType_ = DetailsAssetSelectionTarget::SkeletalMeshMaterial;
			assetSelectionSubMeshIndex_ = static_cast<int>(subMeshIndex);
			EditorContext::Get()->assetSelectorFilter = EditorAssetType::Material;

			AssetSelectorPanel::OnAssetSelected =
				Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

			hud_->ShowPanel<AssetSelectorPanel>();
		}

		ImGui::PopID();

		if (subMeshIndex + 1 < subMeshCount)
		{
			ImGui::Separator();
		}
	}

	ImGui::Text("Render mask: ");

	ImGui::SameLine();

	ImGui::PushItemWidth(100.f);

	if (skeletalMeshInstance)
	{
		int currentValue = skeletalMeshInstance->GetRenderMask();
		EditorWidgets::DrawInputInt("##SkeletalMeshInstanceRenderMask" + specialPostfix, currentValue);
		if (currentValue != static_cast<int>(skeletalMeshInstance->GetRenderMask()))
		{
			skeletalMeshInstance->SetRenderMask(currentValue);
			MarkSceneDirty("Skeletal mesh render mask changed");
		}
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawNavigationTreeComponentDetails(NavigationTreeComponent* navigationTreeComponent)
{
	if (!navigationTreeComponent)
	{
		return;
	}

	const std::string specialPostfix = "##" + std::to_string(navigationTreeComponent->GetGUID());
	const std::string navigationTreePath = navigationTreeComponent->GetNavigationTreePath();

	ImGui::Text("Navigation Tree: ");
	ImGui::SameLine();
	ImGui::TextWrapped("%s", navigationTreePath.c_str());

	const std::string selectAssetLabel = std::string("Select asset") + specialPostfix;
	if (ImGui::Button(selectAssetLabel.c_str()))
	{
		assetSelectionComponent_ = navigationTreeComponent;
		assetSelectionComponentType_ = DetailsAssetSelectionTarget::NavigationTree;
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::NavigationTree;

		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

		hud_->ShowPanel<AssetSelectorPanel>();
	}

	if (!navigationTreePath.empty())
	{
		ImGui::SameLine();
		const std::string clearAssetLabel = std::string("Clear") + specialPostfix;
		if (ImGui::Button(clearAssetLabel.c_str()))
		{
			navigationTreeComponent->SetNavigationTreePath("");
			RebuildSceneNavigationMesh();
			MarkSceneDirty("Navigation tree cleared");
		}
	}

	ImGui::Text("Relative Nodes: %d", static_cast<int>(navigationTreeComponent->GetRelativeNavigationTree().GetNodes().size()));
	ImGui::Text("World Nodes: %d", static_cast<int>(navigationTreeComponent->GetNavigationTree().GetNodes().size()));
}

void DetailsPanel::DrawParticleSystemComponentDetails(ParticleSystemComponent* particleSystemComponent)
{
	const std::string specialPostfix = "##" + std::to_string(particleSystemComponent->GetGUID());
	GPUParticleSpawnDesc spawnDesc = particleSystemComponent->GetSpawnDesc();
	const BillboardParticleSystemComponent* billboardParticleSystemComponent = dynamic_cast<const BillboardParticleSystemComponent*>(particleSystemComponent);
	const StaticMeshParticleSystemComponent* staticMeshParticleSystemComponent = dynamic_cast<const StaticMeshParticleSystemComponent*>(particleSystemComponent);

	ImGui::Text("Render Mode: %s", staticMeshParticleSystemComponent ? "Static Mesh" : "Billboard");
	if (staticMeshParticleSystemComponent)
	{
		ImGui::Text("Static Mesh: ");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", staticMeshParticleSystemComponent->GetStaticMeshPath().empty() ? "" : staticMeshParticleSystemComponent->GetStaticMeshPath().c_str());
	}
	else if (billboardParticleSystemComponent)
	{
		ImGui::Text("Billboard Material: ");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", billboardParticleSystemComponent->GetBillboardMaterialPath().empty() ? "" : billboardParticleSystemComponent->GetBillboardMaterialPath().c_str());
		ImGui::Text("Billboard Texture: ");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", billboardParticleSystemComponent->GetBillboardTexturePath().empty() ? "" : billboardParticleSystemComponent->GetBillboardTexturePath().c_str());
	}

	float particleSize = particleSystemComponent->GetParticleSize();
	ImGui::Text("Particle Size: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##ParticleSystemParticleSize" + specialPostfix, particleSize))
	{
		particleSystemComponent->SetParticleSize(particleSize);
		MarkSceneDirty("Particle size changed");
	}

	float emissiveColorStart[3] = {
		spawnDesc.emissiveColorByLifetime.startValue.x,
		spawnDesc.emissiveColorByLifetime.startValue.y,
		spawnDesc.emissiveColorByLifetime.startValue.z
	};
	ImGui::Text("Emissive Start: ");
	ImGui::SameLine();
	const bool emissiveStartChanged = ImGui::ColorEdit3(("##ParticleSystemEmissiveStart" + specialPostfix).c_str(), emissiveColorStart, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	if (emissiveStartChanged)
	{
		spawnDesc.emissiveColorByLifetime.startValue = Vector3(emissiveColorStart[0], emissiveColorStart[1], emissiveColorStart[2]);
		particleSystemComponent->SetSpawnDesc(spawnDesc);
		MarkSceneDirty("Particle emissive color changed");
	}

	float emissiveColorEnd[3] = {
		spawnDesc.emissiveColorByLifetime.endValue.x,
		spawnDesc.emissiveColorByLifetime.endValue.y,
		spawnDesc.emissiveColorByLifetime.endValue.z
	};
	ImGui::Text("Emissive End: ");
	ImGui::SameLine();
	const bool emissiveEndChanged = ImGui::ColorEdit3(("##ParticleSystemEmissiveEnd" + specialPostfix).c_str(), emissiveColorEnd, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	if (emissiveEndChanged)
	{
		spawnDesc.emissiveColorByLifetime.endValue = Vector3(emissiveColorEnd[0], emissiveColorEnd[1], emissiveColorEnd[2]);
		particleSystemComponent->SetSpawnDesc(spawnDesc);
		MarkSceneDirty("Particle emissive color changed");
	}

	ImGui::Text("Looping: %s", spawnDesc.looping ? "Yes" : "No");
	ImGui::Text("Infinite Lifetime: %s", spawnDesc.infiniteLifetime ? "Yes" : "No");
	ImGui::Text("Initial Size Range: %.3f - %.3f", spawnDesc.initialSize.minValue, spawnDesc.initialSize.maxValue);
	ImGui::Text("Spawn Interval: %.3f", spawnDesc.spawnInterval);
	ImGui::Text("Preview Burst Count: %u", particleSystemComponent->GetPreviewParticleCount());

	if (ImGui::Button(("Open Particle System Panel" + specialPostfix).c_str()))
	{
		hud_->ShowPanel<ParticleSystemPanel>();
	}

	ImGui::SameLine();
	if (ImGui::Button(("Regenerate Preview" + specialPostfix).c_str()))
	{
		particleSystemComponent->RegeneratePreviewParticles();
	}
}

void DetailsPanel::DrawBoxCollisionComponentDetails(BoxCollisionComponent* boxCollisionComponent)
{
	Vector3 currentHalfSize = boxCollisionComponent->GetHalfSize();
	if (EditorWidgets::DrawInputVector3(
		"HalfSize",
		currentHalfSize))
	{
		ObjectBase* owner = boxCollisionComponent->GetOwner();
		const bool wasCollisionDebugEnabled = IsDetailsCollisionDebugEnabled(owner);
		ObjectBase* clone = owner->Clone();
		engine->GetApplication()->GetMainScene()->AddObject(clone);

		EditorContext::Get()->ClearSelection();
		EditorContext::Get()->SetSelection(clone, EditorSelectionType::Object);

		clone->GetFirstComponentOfType<BoxCollisionComponent>()->SetHalfSize(currentHalfSize);

		owner->Destroy();
		engine->GetApplication()->GetMainScene()->RemoveObject(owner);
		if (wasCollisionDebugEnabled)
		{
			DrawDetailsCollisionDebug(clone);
		}

		MarkSceneDirty("Box collision half size changed");
	}
}

void DetailsPanel::DrawSphereCollisionComponentDetails(SphereCollisionComponent* sphereCollisionComponent)
{
	ImGui::Text("Radius: ");
	ImGui::SameLine();
	float sphereCollisionComponentRadius = sphereCollisionComponent->GetRadius();
	EditorWidgets::DrawInputFloat((std::string("##SphereCollisionComponentRadius_") + std::to_string(sphereCollisionComponent->GetRadius())).c_str(), sphereCollisionComponentRadius);

	if (sphereCollisionComponentRadius != sphereCollisionComponent->GetRadius())
	{
		sphereCollisionComponent->SetRadius(sphereCollisionComponentRadius);
		RefreshDetailsCollisionDebug(sphereCollisionComponent->GetOwner());
		MarkSceneDirty("Sphere collision radius changed");
	}
}

void DetailsPanel::DrawCapsuleCollisionComponentDetails(CapsuleCollisionComponent* capsuleCollisionComponent)
{
	ImGui::Text("Radius: ");
	ImGui::SameLine();
	float capsuleCollisionComponentRadius = capsuleCollisionComponent->GetRadius();
	EditorWidgets::DrawInputFloat((std::string("##SphereCollisionComponentRadius_") + std::to_string(capsuleCollisionComponent->GetRadius())).c_str(), capsuleCollisionComponentRadius);

	if (capsuleCollisionComponentRadius != capsuleCollisionComponent->GetRadius())
	{
		capsuleCollisionComponent->SetRadius(capsuleCollisionComponentRadius);
		RefreshDetailsCollisionDebug(capsuleCollisionComponent->GetOwner());
		MarkSceneDirty("Capsule collision radius changed");
	}

	ImGui::Text("Height: ");
	ImGui::SameLine();
	float capsuleCollisionComponentHeight = capsuleCollisionComponent->GetHeight();
	EditorWidgets::DrawInputFloat((std::string("##SphereCollisionComponentHeight_") + std::to_string(capsuleCollisionComponent->GetHeight())).c_str(), capsuleCollisionComponentHeight);

	if (capsuleCollisionComponentHeight != capsuleCollisionComponent->GetHeight())
	{
		capsuleCollisionComponent->SetHeight(capsuleCollisionComponentHeight);
		RefreshDetailsCollisionDebug(capsuleCollisionComponent->GetOwner());
		MarkSceneDirty("Capsule collision height changed");
	}
}

void DetailsPanel::DrawHeightMapCollisionComponentDetails(HeightMapCollisionComponent* heightMapCollisionComponent)
{
	if (!heightMapCollisionComponent)
	{
		return;
	}

	const std::string specialPostfix = "##" + std::to_string(heightMapCollisionComponent->GetGUID());
	const Image* heightMapImage = heightMapCollisionComponent->GetHeightMapImage();

	ImGui::Text("HeightMapImage: ");
	ImGui::SameLine();
	ImGui::TextWrapped("%s", heightMapImage ? ToDisplayContentPath(heightMapImage->GetPath()).c_str() : "");

	if (ImGui::Button(("Select asset" + specialPostfix).c_str()))
	{
		assetSelectionComponent_ = heightMapCollisionComponent;
		assetSelectionComponentType_ = DetailsAssetSelectionTarget::HeightMapImage;
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::Texture;

		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

		hud_->ShowPanel<AssetSelectorPanel>();
	}

	if (heightMapImage)
	{
		ImGui::SameLine();
		if (ImGui::Button(("Clear" + specialPostfix).c_str()))
		{
			heightMapCollisionComponent->SetHeightMapImage(nullptr);
			MarkSceneDirty("Height map image cleared");
		}
	}

	int heightStickWidth = heightMapCollisionComponent->GetHeightStickWidth();
	ImGui::Text("HeightStickWidth: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputInt("##HeightMapCollisionHeightStickWidth" + specialPostfix, heightStickWidth))
	{
		heightMapCollisionComponent->SetHeightStickWidth((std::max)(heightStickWidth, 2));
		MarkSceneDirty("Height map stick width changed");
	}

	int heightStickLength = heightMapCollisionComponent->GetHeightStickLength();
	ImGui::Text("HeightStickLength: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputInt("##HeightMapCollisionHeightStickLength" + specialPostfix, heightStickLength))
	{
		heightMapCollisionComponent->SetHeightStickLength((std::max)(heightStickLength, 2));
		MarkSceneDirty("Height map stick length changed");
	}

	float minHeight = heightMapCollisionComponent->GetMinHeight();
	ImGui::Text("MinHeight: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##HeightMapCollisionMinHeight" + specialPostfix, minHeight))
	{
		heightMapCollisionComponent->SetMinHeight(minHeight);
		MarkSceneDirty("Height map min height changed");
	}

	float maxHeight = heightMapCollisionComponent->GetMaxHeight();
	ImGui::Text("MaxHeight: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##HeightMapCollisionMaxHeight" + specialPostfix, maxHeight))
	{
		heightMapCollisionComponent->SetMaxHeight(maxHeight);
		MarkSceneDirty("Height map max height changed");
	}

	float width = heightMapCollisionComponent->GetWidth();
	ImGui::Text("Width: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##HeightMapCollisionWidth" + specialPostfix, width))
	{
		heightMapCollisionComponent->SetWidth(width);
		MarkSceneDirty("Height map width changed");
	}

	float length = heightMapCollisionComponent->GetLength();
	ImGui::Text("Length: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##HeightMapCollisionLength" + specialPostfix, length))
	{
		heightMapCollisionComponent->SetLength(length);
		MarkSceneDirty("Height map length changed");
	}

	float heightScale = heightMapCollisionComponent->GetHeightScale();
	ImGui::Text("HeightScale: ");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputFloat("##HeightMapCollisionHeightScale" + specialPostfix, heightScale))
	{
		heightMapCollisionComponent->SetHeightScale(heightScale);
		MarkSceneDirty("Height map scale changed");
	}
}

void DetailsPanel::DrawMovingTriangleMeshCollisionComponentDetails(MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const StaticMesh* mesh = movingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");
}

void DetailsPanel::DrawNonMovingTriangleMeshCollisionComponentDetails(NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const StaticMesh* mesh = nonMovingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");

}

void DetailsPanel::DrawPhysicsDetails()
{
	PhysicsObject* physicsObject = EditorContext::Get()->GetSelectionAs<PhysicsObject>();

	char nameBuf[64];
	strcpy(nameBuf, physicsObject->GetName().c_str());
	if (ImGui::InputText("Name", nameBuf, 64))
	{
		physicsObject->SetName(nameBuf);
	}

	DrawTransform();

	ImGui::Separator();
	ImGui::Text("Add Component");
	for (auto& pair : objectReflections_)
	{
		if (ImGui::Button(pair.first.c_str()))
		{
			pair.second(physicsObject);
		}
	}
}

void DetailsPanel::DrawAddComponentOptions(ObjectBase* object)
{
	static const char* objectBaseComponents[]{
		"",
		"StaticMeshComponent",
		"SkeletalMeshComponent",
		"BillboardParticleSystemComponent",
		"StaticMeshParticleSystemComponent",
		"DirectionalLightComponent",
		"PointLightComponent",
		"SpotLightComponent",
		"NavigationTreeComponent"
	};

	static const char* physicsObjectComponents[]{
		"",
		"StaticMeshComponent",
		"SkeletalMeshComponent",
		"BillboardParticleSystemComponent",
		"StaticMeshParticleSystemComponent",
		"DirectionalLightComponent",
		"PointLightComponent",
		"SpotLightComponent",
		"NavigationTreeComponent",
		"BoxCollisionComponent",
		"CapsuleCollisionComponent",
		"SphereCollisionComponent",
		"HeightMapCollisionComponent",
		"MovingTriangleMeshCollisionComponent",
		"NonMovingTriangleMeshCollisionComponent"
	};

	PhysicsObject* physicsObject = dynamic_cast<PhysicsObject*>(object);

	static int selectedItem = 0;
	if (physicsObject)
	{
		bool check = ImGui::Combo("##AddComponent", &selectedItem, physicsObjectComponents, IM_ARRAYSIZE(physicsObjectComponents));
		if (check)
		{
			std::string selectedComponentString = physicsObjectComponents[selectedItem];

			if (objectReflections_.find(selectedComponentString) != objectReflections_.end())
			{
				objectReflections_[selectedComponentString](physicsObject);
				MarkSceneDirty("Component added");
			}
			else if (physicsReflections_.find(selectedComponentString) != physicsReflections_.end())
			{
				const bool wasCollisionDebugEnabled = IsDetailsCollisionDebugEnabled(object);
				physicsReflections_[selectedComponentString](physicsObject);
				if (wasCollisionDebugEnabled)
				{
					RefreshDetailsCollisionDebug(object);
				}
				MarkSceneDirty("Collision component added");
			}
			selectedItem = 0;
		}
	}
	else
	{
		bool check = ImGui::Combo("##AddComponent", &selectedItem, objectBaseComponents, IM_ARRAYSIZE(objectBaseComponents));
		if (check)
		{
			std::string selectedComponentString = objectBaseComponents[selectedItem];

			if (objectReflections_.find(selectedComponentString) != objectReflections_.end())
			{
				objectReflections_[selectedComponentString](object);
				MarkSceneDirty("Component added");
			}

			selectedItem = 0;
		}
	}
}

void DetailsPanel::DrawDirectionalLightDetails()
{
	DirectionalLight* light = EditorContext::Get()->GetSelectionAs<DirectionalLight>();

	static Vector3 lightPosition = light->GetPosition();
	static Vector3 lightDirection = light->GetDirection();
	static Vector3 lightColor = light->GetColor();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	const bool positionChanged = EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	const bool directionChanged = EditorWidgets::DrawInputVector3("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	const bool intensityChanged = EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	const bool colorChanged = EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	const bool shadowEnabledChanged = ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	const bool shadowIntensityChanged = EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	if (positionChanged || directionChanged || intensityChanged || colorChanged || shadowEnabledChanged || shadowIntensityChanged)
	{
		MarkSceneDirty("Directional light changed");
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawPointLightDetails()
{
	PointLight* light = EditorContext::Get()->GetSelectionAs<PointLight>();

	Vector3 lightPosition = light->GetPosition();
	Vector3 lightColor = light->GetColor();
	float lightIntensity = light->GetIntensity();
	float lightRadius = light->GetRadius();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();
	float lightShadowIntensity = light->GetShadowIntensity();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	const bool positionChanged = EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	const bool intensityChanged = EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	const bool colorChanged = EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Radius: ");
	ImGui::SameLine();
	const bool radiusChanged = EditorWidgets::DrawInputFloat("##Radius", lightRadius);
	light->SetRadius(lightRadius);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	const bool shadowEnabledChanged = ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	const bool shadowIntensityChanged = EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	if (positionChanged || intensityChanged || colorChanged || radiusChanged || shadowEnabledChanged || shadowIntensityChanged)
	{
		MarkSceneDirty("Point light changed");
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawSpotLightDetails()
{
	SpotLight* light = EditorContext::Get()->GetSelectionAs<SpotLight>();

	static Vector3 lightDirection = light->GetDirection();
	static Vector3 lightColor = light->GetColor();
	static float lightFalloffAngle = light->GetFalloffAngle();
	static float lightCoverageAngle = light->GetCoverageAngle();

	Vector3 lightPosition = light->GetPosition();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	const bool positionChanged = EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	const bool directionChanged = EditorWidgets::DrawInputVector3("##Direction", lightDirection);
	light->SetDirection(lightDirection.GetNormalized());

	ImGui::Text("FalloffAngle: ");
	ImGui::SameLine();
	const bool falloffChanged = EditorWidgets::DrawInputFloat("##FalloffAngle", lightFalloffAngle);
	light->SetFalloffAngle(lightFalloffAngle);

	ImGui::Text("CoverageAngle: ");
	ImGui::SameLine();
	const bool coverageChanged = EditorWidgets::DrawInputFloat("##CoverageAngle", lightCoverageAngle);
	light->SetCoverageAngle(lightCoverageAngle);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	const bool intensityChanged = EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	const bool colorChanged = EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	const bool shadowEnabledChanged = ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	const bool shadowIntensityChanged = EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	if (positionChanged || directionChanged || falloffChanged || coverageChanged || intensityChanged || colorChanged || shadowEnabledChanged || shadowIntensityChanged)
	{
		MarkSceneDirty("Spot light changed");
	}

	ImGui::PopItemWidth();
}
