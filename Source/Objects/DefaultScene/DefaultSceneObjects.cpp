#include "DefaultSceneObjects.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"

#include "UI/EditorUtils.h"

DefaultSceneArch::DefaultSceneArch()
{
	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneArchStaticMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/DefaultScene/SM_Arch.fbx");
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
	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneCubeStaticMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/DefaultScene/SM_UnitCube.fbx");
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
	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneRadialStaircaseStaticMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/DefaultScene/SM_RadialStaircase.fbx");
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
	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* sceneRadialStaircaseStaticMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/DefaultScene/SM_RadialStaircaseMirrored.fbx");
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
	collisionComponent_ = AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();

	StaticMesh* stairsStaticMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/DefaultScene/SM_RoundCorner.fbx");
	if (stairsStaticMesh)
	{
		collisionComponent_->SetMesh(stairsStaticMesh);
		collisionComponent_->SetCollisionGroup(CollisionGroup::AllBlock);
		collisionComponent_->SetCollisionMask(CollisionMask::Custom0);

		staticMeshComponent_->SetMesh(stairsStaticMesh);
	}
}