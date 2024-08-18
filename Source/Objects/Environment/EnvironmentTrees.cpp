#include "pch.h"

#include "EnvironmentTrees.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"

EnvironmentTree::EnvironmentTree()
{
	collisionComponent_ = AddSubComponent<CapsuleCollisionComponent>();
	collisionComponent_->SetRadius(0.2f);
	collisionComponent_->SetHeight(2.f);
	collisionComponent_->SetRelativePosition(Vector3{ 0.f, 0.f, -1.f });
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	staticMeshComponent_->SetParent(collisionComponent_);
}

void EnvironmentTree::BeginGame()
{
	RigidBody::BeginGame();
}

EnvironmentTree_01::EnvironmentTree_01() : EnvironmentTree()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Trees/SM_Tree_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}