#include "pch.h"

#include "Portal.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Components/LightComponents/PointLightComponent.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"

Portal::Portal() : RigidBody()
{
	SetIsTickable(true);

	ResourceManager* resourceManager = engine->GetResourceManager();

	multipleCollisionComponent_ = AddSubComponent<MultipleCollisionComponent>();
	SetRootComponent(multipleCollisionComponent_);

	StaticMesh* stairsColliderStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Portals/Collisions/SM_Collision_Portal_Outer_01_Stairs.fbx");
	if (stairsColliderStaticMesh)
	{
		MovingTriangleMeshCollisionComponent* stairCollider = AddSubComponent<MovingTriangleMeshCollisionComponent>();

		stairCollider->SetMesh(stairsColliderStaticMesh);
		stairCollider->SetCollisionGroup(CollisionGroup::AllBlock);
		stairCollider->SetCollisionMask(CollisionMask::Custom0);
		multipleCollisionComponent_->AddCollisionComponent(stairCollider);
	}

	BoxCollisionComponent* floorCollider = AddSubComponent<BoxCollisionComponent>();
	floorCollider->SetCollisionGroup(CollisionGroup::AllBlock);
	floorCollider->SetCollisionMask(CollisionMask::Custom0);
	floorCollider->SetRelativePosition(Vector3{ 0.f, 1.65f, 0.f });
	floorCollider->SetRelativeScaling(Vector3{ 2.f, 1.5f, 1.5f });
	multipleCollisionComponent_->AddCollisionComponent(floorCollider);

	BoxCollisionComponent* blockCollider1 = AddSubComponent<BoxCollisionComponent>();
	blockCollider1->SetCollisionGroup(CollisionGroup::AllBlock);
	blockCollider1->SetCollisionMask(CollisionMask::Custom1);
	blockCollider1->SetRelativePosition(Vector3{ -1.f, 1.65f, 0.f });
	blockCollider1->SetRelativeScaling(Vector3{ 0.25f, 1.5f, 5.f });
	multipleCollisionComponent_->AddCollisionComponent(blockCollider1);

	BoxCollisionComponent* blockCollider2 = AddSubComponent<BoxCollisionComponent>();
	blockCollider2->SetCollisionGroup(CollisionGroup::AllBlock);
	blockCollider2->SetCollisionMask(CollisionMask::Custom1);
	blockCollider2->SetRelativePosition(Vector3{ 1.f, 1.65f, 0.f });
	blockCollider2->SetRelativeScaling(Vector3{ 0.25f, 1.5f, 5.f });
	multipleCollisionComponent_->AddCollisionComponent(blockCollider2);

	BoxCollisionComponent* blockCollider3 = AddSubComponent<BoxCollisionComponent>();
	blockCollider3->SetCollisionGroup(CollisionGroup::AllBlock);
	blockCollider3->SetCollisionMask(CollisionMask::Custom1);
	blockCollider3->SetRelativePosition(Vector3{ 0.f, 2.5f, 0.f });
	blockCollider3->SetRelativeScaling(Vector3{ 2.f, 0.25f, 5.f });
	multipleCollisionComponent_->AddCollisionComponent(blockCollider3);

	outerStaticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	outerStaticMeshComponent_->SetParent(GetRootComponent());

	StaticMesh* outerStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Portals/SM_Portal_Outer_01.fbx");
	if (outerStaticMesh)
	{
		outerStaticMeshComponent_->SetMesh(outerStaticMesh);
	}

	portalStaticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	portalStaticMeshComponent_->SetRelativePosition(Vector3{ 0.f, 2.3f, 1.75f });
	portalStaticMeshComponent_->SetParent(GetRootComponent());

	StaticMesh* portalStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Portals/SM_Portal.fbx");
	if (portalStaticMesh)
	{
		portalStaticMeshComponent_->SetMesh(portalStaticMesh);
	}

	lightComponent_ = AddSubComponent<PointLightComponent>();
	lightComponent_->SetRelativePosition(Vector3{ 0.f, 2.3f, 1.75f });
	PointLight* pointLight = lightComponent_->GetPointLight();
	pointLight->SetColor(Vector3{ 0.3f, 1.f, 1.f });
	pointLight->SetIntensity(10.f);
	pointLight->SetRadius(50.f);
	pointLight->SetIsShadowEnabled(false);
}

Portal::~Portal()
{
}

void Portal::BeginGame()
{
	RigidBody::BeginGame();
}

void Portal::Tick(float deltaTime)
{
	RigidBody::Tick(deltaTime);

	Vector3 scaledWorldPosition = Vector3{ 0.f, 2.3f, 1.75f };

	Vector3 origin1 = scaledWorldPosition + Vector3(3.f, 3.f, 1.5f);
	Vector3 origin2 = scaledWorldPosition + Vector3(-3.f, 3.f, -1.5f);
	Vector3 origin3 = scaledWorldPosition + Vector3(3.f, -3.f, 1.5f);
	Vector3 origin4 = scaledWorldPosition + Vector3(-3.f, -3.f, -1.5f);

	float distance1 = origin1.Length();
	float distance2 = origin2.Length();
	float distance3 = origin3.Length();
	float distance4 = origin4.Length();

	float scaledTime = engine->GetElapsedTime() * 2.f;

	float wave =	sin(3.3f * PI * distance1 * 0.13f + scaledTime) * 0.025f +
					sin(3.2f * PI * distance2 * 0.12f + scaledTime) * 0.025f +
					sin(3.1f * PI * distance3 * 0.24f + scaledTime) * 0.025f +
					sin(3.5f * PI * distance4 * 0.32f + scaledTime) * 0.025f;

	lightComponent_->SetRelativePosition(lightComponent_->GetRelativePosition() + Vector3{ -wave * 0.05f });
}

void Portal::OnOverlapBegin(PhysicsObject* otherObject, CollisionComponent* otherComponent, const Vector3& hitPosition, const Vector3& hitNormal)
{
	GOKNAR_INFO("Overlap begin with portal");
}
