#include "pch.h"

#include "Game.h"

#include <chrono>

#include <Goknar.h>
#include <Core.h>

#include "Goknar/Scene.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Managers/ConfigManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Objects/FreeCameraObject.h"
#include "UI/EditorHUD.h"

#include "Physics/RigidBody.h"

#include "Objects/DefaultScene/DefaultSceneObjects.h"

Game::Game() : Application()
{
	REGISTER_CLASS(ObjectBase);
	REGISTER_CLASS(RigidBody);
	REGISTER_CLASS(DefaultSceneArch);
	REGISTER_CLASS(DefaultSceneCube);
	REGISTER_CLASS(DefaultSceneRadialStaircase);
	REGISTER_CLASS(DefaultSceneRadialStaircaseMirrored);
	REGISTER_CLASS(DefaultSceneRoundCorner);
	
	engine->SetApplication(this);

	ContentDir = "EditorContent/";
	AssetParser::ParseAssets("EditorAssetContainer");

	RenderPassType mainRenderType = RenderPassType::Deferred;

	std::chrono::steady_clock::time_point lastFrameTimePoint = std::chrono::steady_clock::now();

	ConfigManager editorConfig;
	std::string currentProjectPath = "./";
	if (editorConfig.ReadFile("Config/EditorConfig.ini"))
	{
		currentProjectPath = editorConfig.GetString("Editor", "CurrentProjectPath", "");
		ProjectDir = currentProjectPath;
		LoadProject(currentProjectPath);
	}
	else
	{
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

			engine->GetRenderer()->SetMainRenderType(mainRenderType);

			std::string contentDir = config.GetString("Core", "ContentDir", "Content/");

			ContentDir = contentDir;

			std::string mainScenePath = config.GetString("Core", "MainScene", "Scenes/DefaultScene.xml");
			mainScene_->ReadSceneData(mainScenePath);

			editorHUD_ = new EditorHUD();
		}
		else
		{
			GOKNAR_CORE_ERROR("Failed to load GameConfig.ini. Falling back to defaults.");
		}
	}

	AssetParser::ParseAssets("AssetContainer");

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

void Game::LoadProject(const std::string& projectPath)
{
	ConfigManager config;
	std::string configPath = projectPath + "Config/GameConfig.ini";
	if (config.ReadFile(configPath))
	{
		int width = config.GetInt("Graphics", "WindowWidth", 1920);
		int height = config.GetInt("Graphics", "WindowHeight", 1080);
		engine->GetWindowManager()->SetWindowSize(width, height);

		std::string mainRenderTypeStr = config.GetString("Graphics", "MainRenderType", "Deferred");
		RenderPassType mainRenderType = RenderPassType::Deferred;
		if (mainRenderTypeStr == "Forward")
		{
			mainRenderType = RenderPassType::Forward;
		}

		engine->GetRenderer()->SetMainRenderType(mainRenderType);

		std::string contentDir = config.GetString("Core", "ContentDir", "Content/");
		ContentDir = projectPath + contentDir;

		std::string mainScenePath = config.GetString("Core", "MainScene", "Scenes/DefaultScene.xml");
		mainScene_->ReadSceneData(mainScenePath);
	}
	else
	{
		GOKNAR_CORE_ERROR("Failed to load {}. Falling back to defaults.", configPath);
	}
}

Application *CreateApplication()
{
	return new Game();
}
