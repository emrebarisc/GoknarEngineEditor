#include "DetailsPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "UI/EditorHUD.h"
#include "UI/EditorWidgets.h"
#include "UI/Panels/AssetSelectorPanel.h"

template <class T>
void AddCollisionComponent(PhysicsObject* physicsObject)
{
	if (physicsObject->GetFirstComponentOfType<CollisionComponent>())
	{
		return;
	}

	T* collisionComponent = physicsObject->AddSubComponent<T>();
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
	switch (assetSelectionComponentType_)
	{
	case EditorAssetType::None:
		break;
	case EditorAssetType::StaticMeshComponent:
	{
		StaticMeshComponent* staticMeshComponent = (StaticMeshComponent*)assetSelectionComponent_;
		StaticMesh* newStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(path);
		if (newStaticMesh)
		{
			staticMeshComponent->SetMesh(newStaticMesh);
		}
		break;
	}
	case EditorAssetType::SkeletalMeshComponent:
		break;
	case EditorAssetType::Image:
		break;
	case EditorAssetType::Audio:
		break;
	default: 
		break;
	}

	assetSelectionComponent_ = nullptr;
	assetSelectionComponentType_ = EditorAssetType::None;
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
	EditorWidgets::DrawInputVector3("##Position", position);
	object->SetWorldPosition(position);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Rotation", rotation);
	object->SetWorldRotation(Quaternion::FromEulerDegrees(rotation));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Scaling", scale);
	object->SetWorldScaling(scale);
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
	EditorWidgets::DrawInputText("##Name", newName);
	if (newName != selectedObjectName)
	{
		object->SetName(newName);
	}

	ImGui::Text("Position: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Position", selectedObjectWorldPosition);
	object->SetWorldPosition(selectedObjectWorldPosition);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Rotation", selectedObjectWorldRotationEulerDegrees);
	object->SetWorldRotation(Quaternion::FromEulerDegrees(selectedObjectWorldRotationEulerDegrees));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Scaling", selectedObjectWorldScaling);
	object->SetWorldScaling(selectedObjectWorldScaling);

	ImGui::Separator();
	RigidBody* rigidBody = dynamic_cast<RigidBody*>(object);
	if (rigidBody)
	{
		float rigidBodyMass = rigidBody->GetMass();
		ImGui::Text("Mass: ");
		ImGui::SameLine();
		EditorWidgets::DrawInputFloat("##Mass", rigidBodyMass);
		rigidBody->SetMass(rigidBodyMass);

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
			CollisionGroup collisionGroup = CollisionGroup::Default;
			if (selectedCollisionGroup == 2)
			{
				collisionGroup = CollisionGroup::WorldStaticBlock;
			}
			else if (selectedCollisionGroup == 3)
			{
				collisionGroup = CollisionGroup::WorldDynamicBlock;
			}
			else if (selectedCollisionGroup == 4)
			{
				collisionGroup = CollisionGroup::AllBlock;
			}
			else if (selectedCollisionGroup == 5)
			{
				collisionGroup = CollisionGroup::WorldDynamicOverlap;
			}
			else if (selectedCollisionGroup == 6)
			{
				collisionGroup = CollisionGroup::WorldStaticOverlap;
			}
			else if (selectedCollisionGroup == 7)
			{
				collisionGroup = CollisionGroup::AllOverlap;
			}
			else if (selectedCollisionGroup == 8)
			{
				collisionGroup = CollisionGroup::Character;
			}
			else if (selectedCollisionGroup == 9)
			{
				collisionGroup = CollisionGroup::All;
			}
			else if (selectedCollisionGroup == 10)
			{
				collisionGroup = CollisionGroup::Custom0;
			}
			else if (selectedCollisionGroup == 11)
			{
				collisionGroup = CollisionGroup::Custom1;
			}
			else if (selectedCollisionGroup == 12)
			{
				collisionGroup = CollisionGroup::Custom2;
			}
			else if (selectedCollisionGroup == 13)
			{
				collisionGroup = CollisionGroup::Custom3;
			}
			else if (selectedCollisionGroup == 14)
			{
				collisionGroup = CollisionGroup::Custom4;
			}
			else if (selectedCollisionGroup == 15)
			{
				collisionGroup = CollisionGroup::Custom5;
			}
			else if (selectedCollisionGroup == 16)
			{
				collisionGroup = CollisionGroup::Custom6;
			}
			else if (selectedCollisionGroup == 17)
			{
				collisionGroup = CollisionGroup::Custom7;
			}
			else if (selectedCollisionGroup == 18)
			{
				collisionGroup = CollisionGroup::Custom8;
			}
			else if (selectedCollisionGroup == 19)
			{
				collisionGroup = CollisionGroup::Custom9;
			}
			rigidBody->SetCollisionGroup(collisionGroup);
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
			CollisionMask collisionMask = CollisionMask::Default;
			if (selectedCollisionMask == 2)
			{
				collisionMask = CollisionMask::BlockWorldDynamic;
			}
			else if (selectedCollisionMask == 3)
			{
				collisionMask = CollisionMask::BlockWorldStatic;
			}
			else if (selectedCollisionMask == 4)
			{
				collisionMask = CollisionMask::BlockCharacter;
			}
			else if (selectedCollisionMask == 5)
			{
				collisionMask = CollisionMask::BlockAll;
			}
			else if (selectedCollisionMask == 6)
			{
				collisionMask = CollisionMask::BlockAllExceptCharacter;
			}
			else if (selectedCollisionMask == 7)
			{
				collisionMask = CollisionMask::OverlapWorldDynamic;
			}
			else if (selectedCollisionMask == 8)
			{
				collisionMask = CollisionMask::OverlapWorldStatic;
			}
			else if (selectedCollisionMask == 9)
			{
				collisionMask = CollisionMask::OverlapCharacter;
			}
			else if (selectedCollisionMask == 10)
			{
				collisionMask = CollisionMask::OverlapAll;
			}
			else if (selectedCollisionMask == 11)
			{
				collisionMask = CollisionMask::OverlapAllExceptCharacter;
			}
			else if (selectedCollisionMask == 12)
			{
				collisionMask = CollisionMask::BlockAndOverlapAll;
			}
			else if (selectedCollisionMask == 13)
			{
				collisionMask = CollisionMask::Custom0;
			}
			else if (selectedCollisionMask == 14)
			{
				collisionMask = CollisionMask::Custom1;
			}
			else if (selectedCollisionMask == 15)
			{
				collisionMask = CollisionMask::Custom2;
			}
			else if (selectedCollisionMask == 16)
			{
				collisionMask = CollisionMask::Custom3;
			}
			else if (selectedCollisionMask == 17)
			{
				collisionMask = CollisionMask::Custom4;
			}
			else if (selectedCollisionMask == 18)
			{
				collisionMask = CollisionMask::Custom5;
			}
			else if (selectedCollisionMask == 19)
			{
				collisionMask = CollisionMask::Custom6;
			}
			else if (selectedCollisionMask == 20)
			{
				collisionMask = CollisionMask::Custom7;
			}
			else if (selectedCollisionMask == 21)
			{
				collisionMask = CollisionMask::Custom8;
			}
			else if (selectedCollisionMask == 22)
			{
				collisionMask = CollisionMask::Custom9;
			}
			rigidBody->SetCollisionMask(collisionMask);
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

void DetailsPanel::DrawComponentDetails(ObjectBase* owner, Component* component)
{
	if (!component)
	{
		return;
	}

	StaticMeshComponent* staticMeshComponent{ nullptr };
	BoxCollisionComponent* boxCollisionComponent{ nullptr };
	SphereCollisionComponent* sphereCollisionComponent{ nullptr };
	CapsuleCollisionComponent* capsuleCollisionComponent{ nullptr };
	MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent{ nullptr };
	NonMovingTriangleMeshCollisionComponent* nonMvingTriangleMeshCollisionComponent{ nullptr };

	std::string componentTypeString;

	if (staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component))
	{
		componentTypeString = "StaticMeshComponent";
	}
	else if (boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component))
	{
		componentTypeString = "BoxCollisionComponent";
	}
	else if (sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component))
	{
		componentTypeString = "SphereCollisionComponent";
	}
	else if (capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
	{
		componentTypeString = "CapsuleCollisionComponent";
	}
	else if (movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
	{
		componentTypeString = "MovingTriangleMeshCollisionComponent";
	}
	else if (nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
	{
		componentTypeString = "NonMovingTriangleMeshCollisionComponent";
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
	EditorWidgets::DrawInputVector3(std::string("##RelativePosition") + specialPostfix, componentRelativePosition);
	component->SetRelativePosition(componentRelativePosition);

	ImGui::Text("RelativeRotation: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3(std::string("##RelativeRotation") + specialPostfix, componentRelativeRotationEulerDegrees);
	component->SetRelativeRotation(Quaternion::FromEulerDegrees(componentRelativeRotationEulerDegrees));

	ImGui::Text("RelativeScaling: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3(std::string("##RelativeScaling") + specialPostfix, componentRelativeScaling);
	component->SetRelativeScaling(componentRelativeScaling);

	if (staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component))
	{
		DrawStaticMeshComponentDetails(staticMeshComponent);
	}
	else if (boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component))
	{
		DrawBoxCollisionComponentDetails(boxCollisionComponent);
	}
	else if (sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component))
	{
		DrawSphereCollisionComponentDetails(sphereCollisionComponent);
	}
	else if (capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
	{
		DrawCapsuleCollisionComponentDetails(capsuleCollisionComponent);
	}
	else if (movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
	{
		DrawMovingTriangleMeshCollisionComponentDetails(movingTriangleMeshCollisionComponent);
	}
	else if (nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
	{
		DrawNonMovingTriangleMeshCollisionComponentDetails(nonMvingTriangleMeshCollisionComponent);
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
		assetSelectionComponentType_ = EditorAssetType::StaticMeshComponent;

		AssetSelectorPanel::OnAssetSelected = 
			Delegate<void(const std::string&)>::Create<DetailsPanel, &DetailsPanel::OnAssetSelected>(this);

		hud_->ShowPanel<AssetSelectorPanel>();
	}

	ImGui::Text("Render mask: ");

	ImGui::SameLine();
	StaticMeshInstance* staticMeshInstance = staticMeshComponent->GetMeshInstance();

	ImGui::PushItemWidth(100.f);

	int currentValue = staticMeshInstance->GetRenderMask();
	EditorWidgets::DrawInputInt("##StaticMeshInstanceRenderMask", currentValue);
	if (currentValue != staticMeshInstance->GetRenderMask())
	{
		staticMeshInstance->SetRenderMask(currentValue);
	}

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawBoxCollisionComponentDetails(BoxCollisionComponent* boxCollisionComponent)
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
	}

	ImGui::Text("Height: ");
	ImGui::SameLine();
	float capsuleCollisionComponentHeight = capsuleCollisionComponent->GetHeight();
	EditorWidgets::DrawInputFloat((std::string("##SphereCollisionComponentHeight_") + std::to_string(capsuleCollisionComponent->GetHeight())).c_str(), capsuleCollisionComponentHeight);

	if (capsuleCollisionComponentHeight != capsuleCollisionComponent->GetHeight())
	{
		capsuleCollisionComponent->SetHeight(capsuleCollisionComponentHeight);
	}
}

void DetailsPanel::DrawMovingTriangleMeshCollisionComponentDetails(MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const MeshUnit* mesh = movingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");
}

void DetailsPanel::DrawNonMovingTriangleMeshCollisionComponentDetails(NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const MeshUnit* mesh = nonMovingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");

}

void DetailsPanel::DrawPhysicsDetails()
{
	PhysicsObject* physicsObject = EditorContext::Get()->GetSelectionAs<PhysicsObject>();

	char nameBuf[64];
	strcpy_s(nameBuf, physicsObject->GetName().c_str());
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
		"StaticMeshComponent"
	};

	static const char* physicsObjectComponents[]{
		"",
		"StaticMeshComponent",
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
			}
			else if (physicsReflections_.find(selectedComponentString) != physicsReflections_.end())
			{
				physicsReflections_[selectedComponentString](physicsObject);
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
			}
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
	EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

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
	EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Radius: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Radius", lightRadius);
	light->SetRadius(lightRadius);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}

void DetailsPanel::DrawSpotLightDetails()
{
	SpotLight* light = EditorContext::Get()->GetSelectionAs<SpotLight>();

	Vector3 lightPosition = light->GetPosition();
	Vector3 lightDirection = light->GetDirection();
	Vector3 lightColor = light->GetColor();
	float lightFalloffAngle = light->GetFalloffAngle();
	float lightCoverageAngle = light->GetCoverageAngle();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("FalloffAngle: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##FalloffAngle", lightFalloffAngle);
	light->SetFalloffAngle(lightFalloffAngle);

	ImGui::Text("CoverageAngle: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##CoverageAngle", lightCoverageAngle);
	light->SetCoverageAngle(lightCoverageAngle);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputVector3("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	ImGui::Checkbox("##IsCastingShadow", &lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	EditorWidgets::DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}
