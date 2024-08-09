#include "pch.h"

#include "EnvironmentStones.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"

EnvironmentStone::EnvironmentStone()
{
	collisionComponent_ = AddSubComponent<MovingTriangleMeshCollisionComponent>();
	SetRootComponent(collisionComponent_);

	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	staticMeshComponent_->SetParent(collisionComponent_);
}

EnvironmentStone_01::EnvironmentStone_01() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_Stone_01.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentStone_02::EnvironmentStone_02() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_02.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_Stone_02.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentStone_03::EnvironmentStone_03() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_03.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_Stone_03.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentStone_04::EnvironmentStone_04() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_04.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_Stone_04.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_01::EnvironmentLStone_01() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_01.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_02::EnvironmentLStone_02() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_02.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_02.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_03::EnvironmentLStone_03() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_03.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_03.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_04::EnvironmentLStone_04() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_04.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_04.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_05::EnvironmentLStone_05() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_05.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_05.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}

EnvironmentLStone_06::EnvironmentLStone_06() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_06.fbx");
	staticMeshComponent_->SetMesh(staticMesh);

	StaticMesh* collisionStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/Collisions/SM_Collision_LStone_06.fbx");
	collisionComponent_->SetMesh(collisionStaticMesh);
}