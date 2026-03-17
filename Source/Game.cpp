#include "pch.h"

#include "Game.h"

#include <chrono>

#include <Goknar.h>
#include <Core.h>

#include "Goknar/Scene.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Managers/ConfigManager.h"
#include "Goknar/Managers/WindowManager.h"
#include "Data/MaterialInitializer.h"

#include "Objects/FreeCameraObject.h"
#include "UI/EditorHUD.h"

#include "Physics/RigidBody.h"

#include "Objects/Environment/EnvironmentStones.h"
#include "Objects/Environment/EnvironmentTrees.h"
#include "Objects/Environment/EnvironmentPlants.h"
#include "Objects/DefaultScene/DefaultSceneObjects.h"

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
	REGISTER_CLASS(DefaultSceneArch);
	REGISTER_CLASS(DefaultSceneCube);
	REGISTER_CLASS(DefaultSceneRadialStaircase);
	REGISTER_CLASS(DefaultSceneRadialStaircaseMirrored);
	REGISTER_CLASS(DefaultSceneRoundCorner);

	MaterialInitializer::Init();
	
	engine->SetApplication(this);

	RenderPassType mainRenderType = RenderPassType::Deferred;

	std::chrono::steady_clock::time_point lastFrameTimePoint = std::chrono::steady_clock::now();

	ConfigManager config;
	if (config.ReadFile("Config/GameConfig.ini"))
	{
		int width = config.GetInt("Graphics", "WindowWidth", 1920);
		int height = config.GetInt("Graphics", "WindowHeight", 1080);
		engine->GetWindowManager()->SetWindowSize(width, height);

		std::string mainRenderTypeStr = config.GetString("Graphics", "MainRenderType", "Deferred");
		if (mainRenderTypeStr == "Forward")
		{
			mainRenderType = RenderPassType::Forward;
		}
		else if (mainRenderTypeStr == "Deferred")
		{
			mainRenderType = RenderPassType::Deferred;
		}
		else
		{
			GOKNAR_CORE_WARN("Unknown MainRenderType: {}. Falling back to Deferred.", mainRenderTypeStr);
			mainRenderType = RenderPassType::Deferred;
		}

		std::string contentDir = config.GetString("Core", "ContentDir", CONTENT_DIR);

		ContentDir = contentDir;

		std::string mainScenePath = config.GetString("Core", "MainScene", "Scenes/DefaultScene.xml");
		mainScene_->ReadSceneData(mainScenePath);
	}
	else
	{
		GOKNAR_CORE_ERROR("Failed to load GameConfig.ini. Falling back to defaults.");
	}

	engine->GetRenderer()->SetMainRenderType(mainRenderType);

	std::chrono::steady_clock::time_point currentTimePoint = std::chrono::steady_clock::now();
	float elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTimePoint - lastFrameTimePoint).count();
	GOKNAR_CORE_WARN("Scene is read in {} seconds.", elapsedTime);

	lastFrameTimePoint = currentTimePoint;

	editorHUD_ = new EditorHUD();
	editorHUD_->SetName("__Editor__HUD");
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
