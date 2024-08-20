#include "pch.h"

#include "EnvironmentStones.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"

#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/PhysicsDebugger.h"

EnvironmentStone::EnvironmentStone() : ObjectBase()
{
	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	SetRootComponent(staticMeshComponent_);
}

void EnvironmentStone::BeginGame()
{
	ObjectBase::BeginGame();
}

EnvironmentStone_01::EnvironmentStone_01() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentStone_02::EnvironmentStone_02() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_02.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentStone_03::EnvironmentStone_03() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_03.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentStone_04::EnvironmentStone_04() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_Stone_04.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_01::EnvironmentLStone_01() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_02::EnvironmentLStone_02() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_02.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_03::EnvironmentLStone_03() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_03.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_04::EnvironmentLStone_04() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_04.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_05::EnvironmentLStone_05() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_05.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentLStone_06::EnvironmentLStone_06() : EnvironmentStone()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Stones/SM_LStone_06.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}