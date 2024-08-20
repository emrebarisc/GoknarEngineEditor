#include "pch.h"

#include "Game.h"

#include <chrono>

#include <Goknar.h>
#include <Core.h>

#include "Goknar/Scene.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Camera.h"
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/WindowManager.h"
#include "Data/MaterialInitializer.h"

#include "Factories/DynamicObjectFactory.h"
#include "Objects/FreeCameraObject.h"
#include "UI/EditorHUD.h"

#include "Physics/RigidBody.h"

#include "Objects/Environment/EnvironmentStones.h"
#include "Objects/Environment/EnvironmentTrees.h"
#include "Objects/Environment/EnvironmentPlants.h"

Game::Game() : Application()
{
	REGISTER_CLASS(ObjectBase);
	REGISTER_CLASS(RigidBody);
	REGISTER_CLASS(EnvironmentStone_01);
	REGISTER_CLASS(EnvironmentStone_02);
	REGISTER_CLASS(EnvironmentStone_03);
	REGISTER_CLASS(EnvironmentStone_04);
	REGISTER_CLASS(EnvironmentLStone_01);
	REGISTER_CLASS(EnvironmentLStone_02);
	REGISTER_CLASS(EnvironmentLStone_03);
	REGISTER_CLASS(EnvironmentLStone_04);
	REGISTER_CLASS(EnvironmentLStone_05);
	REGISTER_CLASS(EnvironmentLStone_06);
	REGISTER_CLASS(EnvironmentTree_01);
	REGISTER_CLASS(EnvironmentGrass_01);
	REGISTER_CLASS(EnvironmentMushrooms_01);

	materialInitializer_ = new MaterialInitializer();
	
	engine->SetApplication(this);

	engine->GetRenderer()->SetMainRenderType(RenderPassType::Deferred);

	std::chrono::steady_clock::time_point lastFrameTimePoint = std::chrono::steady_clock::now();
	mainScene_->ReadSceneData("Scenes/Scene.xml");

	std::chrono::steady_clock::time_point currentTimePoint = std::chrono::steady_clock::now();
	float elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTimePoint - lastFrameTimePoint).count();
	GOKNAR_CORE_WARN("Scene is read in {} seconds.", elapsedTime);

	lastFrameTimePoint = currentTimePoint;

	editorHUD_ = new EditorHUD();
	editorHUD_->SetName("__Editor__HUD");

	freeCameraObject_ = new FreeCameraObject();
	freeCameraObject_->SetName("__Editor__Camera");
}

Game::~Game()
{
	delete materialInitializer_;
}

void Game::Run()
{
}

Application *CreateApplication()
{
	return new Game();
}
