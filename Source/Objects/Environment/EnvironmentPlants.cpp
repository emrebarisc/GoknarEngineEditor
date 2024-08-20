#include "pch.h"

#include "EnvironmentPlants.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/ResourceManager.h"

EnvironmentPlant::EnvironmentPlant() : ObjectBase()
{
	staticMeshComponent_ = AddSubComponent<StaticMeshComponent>();
	SetRootComponent(staticMeshComponent_);
}

void EnvironmentPlant::BeginGame()
{
	ObjectBase::BeginGame();
}

EnvironmentGrass_01::EnvironmentGrass_01() : EnvironmentPlant()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Plants/SM_Grass.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}

EnvironmentMushrooms_01::EnvironmentMushrooms_01()
{
	StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>("Meshes/Environment/Plants/SM_Mushrooms_01.fbx");
	staticMeshComponent_->SetMesh(staticMesh);
}
