#include "DetailsPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Application.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Objects/ReflectionProbeObject.h"
#include "Goknar/Components/ParticleSystemComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Managers/ConfigManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/SkeletalMeshInstance.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
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
#include "UI/EditorWidgets.h"
#include "UI/Panels/AssetSelectorPanel.h"
#include "UI/Panels/ParticleSystemPanel.h"

template <class T>
void AddCollisionComponent(PhysicsObject* physicsObject)
{
	if (physicsObject->GetFirstComponentOfType<CollisionComponent>())
	{
		return;
	}

	physicsObject->AddSubComponent<T>();
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

	objectReflections_["NavigationTreeComponent"] =
		[](ObjectBase* objectBase)
		{
			objectBase->AddSubComponent<NavigationTreeComponent>();
		};

	physicsReflections_["BoxCollisionComponent"] =
		[this](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<BoxCollisionComponent>(physicsObject);
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
	physicsReflections_["MovingTriangleMeshCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<MovingTriangleMeshCollisionComponent>(physicsObject);
		};
	physicsReflections_["NonMovingTriangleMeshCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<NonMovingTriangleMeshCollisionComponent>(physicsObject);
		};
}

void DetailsPanel::OnAssetSelected(const std::string& path)
{
	std::string normalizedPath = EditorAssetPathUtils::ToContentRelativePath(path);

	switch (assetSelectionComponentType_)
	{
	case DetailsAssetSelectionTarget::None:
		break;
	case DetailsAssetSelectionTarget::StaticMesh:
	{
		StaticMeshComponent* staticMeshComponent = (StaticMeshComponent*)assetSelectionComponent_;
		StaticMesh* newStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(normalizedPath);
		if (newStaticMesh)
		{
			staticMeshComponent->SetMesh(newStaticMesh);
			MarkSceneDirty("Static mesh changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::Material:
	{
		StaticMeshComponent* staticMeshComponent = (StaticMeshComponent*)assetSelectionComponent_;
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
		break;
	}
	case DetailsAssetSelectionTarget::SkeletalMesh:
	{
		SkeletalMeshComponent* skeletalMeshComponent = (SkeletalMeshComponent*)assetSelectionComponent_;
		SkeletalMesh* newSkeletalMesh = engine->GetResourceManager()->GetContent<SkeletalMesh>(normalizedPath);
		if (skeletalMeshComponent && newSkeletalMesh)
		{
			skeletalMeshComponent->SetMesh(newSkeletalMesh);
			MarkSceneDirty("Skeletal mesh changed");
		}
		break;
	}
	case DetailsAssetSelectionTarget::SkeletalMeshMaterial:
	{
		SkeletalMeshComponent* skeletalMeshComponent = (SkeletalMeshComponent*)assetSelectionComponent_;
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
		break;
	}
	case DetailsAssetSelectionTarget::NavigationTree:
	{
		NavigationTreeComponent* navigationTreeComponent = (NavigationTreeComponent*)assetSelectionComponent_;
		if (navigationTreeComponent)
		{
			const std::string previousPath = navigationTreeComponent->GetNavigationTreePath();
			const bool loadedNavigationTree = navigationTreeComponent->SetNavigationTreePath(normalizedPath);
			if (loadedNavigationTree && previousPath != navigationTreeComponent->GetNavigationTreePath())
			{
				RebuildSceneNavigationMesh();
				MarkSceneDirty("Navigation tree changed");
			}
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
		DrawObjectDetails();
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
	object->SetWorldPosition(position);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	const bool rotationChanged = EditorWidgets::DrawInputVector3("##Rotation", rotation);
	object->SetWorldRotation(Quaternion::FromEulerDegrees(rotation));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	const bool scalingChanged = EditorWidgets::DrawInputVector3("##Scaling", scale);
	object->SetWorldScaling(scale);

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
	object->SetWorldPosition(selectedObjectWorldPosition);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	const bool rotationChanged = EditorWidgets::DrawInputVector3("##Rotation", selectedObjectWorldRotationEulerDegrees);
	object->SetWorldRotation(Quaternion::FromEulerDegrees(selectedObjectWorldRotationEulerDegrees));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	const bool scalingChanged = EditorWidgets::DrawInputVector3("##Scaling", selectedObjectWorldScaling);
	object->SetWorldScaling(selectedObjectWorldScaling);

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
	StaticMeshComponent* staticMeshComponent{ nullptr };
	SkeletalMeshComponent* skeletalMeshComponent{ nullptr };
	BoxCollisionComponent* boxCollisionComponent{ nullptr };
	SphereCollisionComponent* sphereCollisionComponent{ nullptr };
	CapsuleCollisionComponent* capsuleCollisionComponent{ nullptr };
	MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent{ nullptr };
	NonMovingTriangleMeshCollisionComponent* nonMvingTriangleMeshCollisionComponent{ nullptr };
	NavigationTreeComponent* navigationTreeComponent{ nullptr };

	std::string componentTypeString;

	particleSystemComponent = dynamic_cast<ParticleSystemComponent*>(component);
	staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component);
	skeletalMeshComponent = dynamic_cast<SkeletalMeshComponent*>(component);
	boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component);
	sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component);
	capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component);
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

	if (component->GetOwner()->GetRootComponent() == component)
	{
		componentTypeString += " (RootComponent)";
	}

	ImGui::Text(componentTypeString.c_str());

	ImGui::Separator();

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
	}

	particleSystemComponent = dynamic_cast<ParticleSystemComponent*>(component);
	staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component);
	skeletalMeshComponent = dynamic_cast<SkeletalMeshComponent*>(component);
	boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component);
	sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component);
	capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component);
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

void DetailsPanel::DrawBoxCollisionComponentDetails(BoxCollisionComponent*)
{
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
		MarkSceneDirty("Capsule collision radius changed");
	}

	ImGui::Text("Height: ");
	ImGui::SameLine();
	float capsuleCollisionComponentHeight = capsuleCollisionComponent->GetHeight();
	EditorWidgets::DrawInputFloat((std::string("##SphereCollisionComponentHeight_") + std::to_string(capsuleCollisionComponent->GetHeight())).c_str(), capsuleCollisionComponentHeight);

	if (capsuleCollisionComponentHeight != capsuleCollisionComponent->GetHeight())
	{
		capsuleCollisionComponent->SetHeight(capsuleCollisionComponentHeight);
		MarkSceneDirty("Capsule collision height changed");
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
		"NavigationTreeComponent"
	};

	static const char* physicsObjectComponents[]{
		"",
		"StaticMeshComponent",
		"SkeletalMeshComponent",
		"BillboardParticleSystemComponent",
		"StaticMeshParticleSystemComponent",
		"NavigationTreeComponent",
		"BoxCollisionComponent",
		"CapsuleCollisionComponent",
		"SphereCollisionComponent",
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
				physicsReflections_[selectedComponentString](physicsObject);
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
