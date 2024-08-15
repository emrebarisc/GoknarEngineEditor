#include "pch.h"

#include "Game.h"

#include <chrono>

#include <Goknar.h>

#include "Goknar/Scene.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Camera.h"
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Factories/DynamicObjectFactory.h"
#include "Objects/FreeCameraObject.h"
#include "UI/EditorHUD.h"

#include "Physics/RigidBody.h"

#include "Objects/Environment/EnvironmentStones.h"
#include "Objects/Environment/EnvironmentTrees.h"

Game::Game() : Application()
{
	DynamicObjectFactory* dynamicObjectFactory = DynamicObjectFactory::GetInstance();
	dynamicObjectFactory->RegisterClass("ObjectBase", []() -> ObjectBase* { return new ObjectBase(); });
	dynamicObjectFactory->RegisterClass("RigidBody", []() -> RigidBody* { return new RigidBody(); }); 
	dynamicObjectFactory->RegisterClass("Stone_01", []() -> EnvironmentStone* { return new EnvironmentStone_01(); });
	dynamicObjectFactory->RegisterClass("Stone_02", []() -> EnvironmentStone* { return new EnvironmentStone_02(); });
	dynamicObjectFactory->RegisterClass("Stone_03", []() -> EnvironmentStone* { return new EnvironmentStone_03(); });
	dynamicObjectFactory->RegisterClass("Stone_04", []() -> EnvironmentStone* { return new EnvironmentStone_04(); });
	dynamicObjectFactory->RegisterClass("LargeStone_01", []() -> EnvironmentStone* { return new EnvironmentLStone_01(); });
	dynamicObjectFactory->RegisterClass("LargeStone_02", []() -> EnvironmentStone* { return new EnvironmentLStone_02(); });
	dynamicObjectFactory->RegisterClass("LargeStone_03", []() -> EnvironmentStone* { return new EnvironmentLStone_03(); });
	dynamicObjectFactory->RegisterClass("LargeStone_04", []() -> EnvironmentStone* { return new EnvironmentLStone_04(); });
	dynamicObjectFactory->RegisterClass("LargeStone_05", []() -> EnvironmentStone* { return new EnvironmentLStone_05(); });
	dynamicObjectFactory->RegisterClass("LargeStone_06", []() -> EnvironmentStone* { return new EnvironmentLStone_06(); });
	dynamicObjectFactory->RegisterClass("Tree_01", []() -> EnvironmentTree* { return new EnvironmentTree_01(); });

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
}

void Game::Run()
{
}

Application *CreateApplication()
{
	return new Game();
}
