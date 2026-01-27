#include "DefaultSceneObjects.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"

DefaultSceneArch::DefaultSceneArch()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneArchStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/DefaultScene/SM_Arch.fbx");
	if (sceneArchStaticMesh)
	{
		collisionComponent_->SetMesh(sceneArchStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);
		
		staticMeshComponent_->SetMesh(sceneArchStaticMesh);
	}
}

DefaultSceneCube::DefaultSceneCube()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneCubeStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/DefaultScene/SM_UnitCube.fbx");
	if (sceneCubeStaticMesh)
	{
		collisionComponent_->SetMesh(sceneCubeStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);

		staticMeshComponent_->SetMesh(sceneCubeStaticMesh);
	}
}

DefaultSceneRadialStaircase::DefaultSceneRadialStaircase()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneRadialStaircaseStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/DefaultScene/SM_RadialStaircase.fbx");
	if (sceneRadialStaircaseStaticMesh)
	{
		collisionComponent_->SetMesh(sceneRadialStaircaseStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);

		staticMeshComponent_->SetMesh(sceneRadialStaircaseStaticMesh);
	}
}

DefaultSceneRadialStaircaseMirrored::DefaultSceneRadialStaircaseMirrored()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneRadialStaircaseStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/DefaultScene/SM_RadialStaircaseMirrored.fbx");
	if (sceneRadialStaircaseStaticMesh)
	{
		collisionComponent_->SetMesh(sceneRadialStaircaseStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);

		staticMeshComponent_->SetMesh(sceneRadialStaircaseStaticMesh);
	}
}


DefaultSceneRoundCorner::DefaultSceneRoundCorner()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* stairsStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/DefaultScene/SM_RoundCorner.fbx");
	if (stairsStaticMesh)
	{
		collisionComponent_->SetMesh(stairsStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);

		staticMeshComponent_->SetMesh(stairsStaticMesh);
	}
}