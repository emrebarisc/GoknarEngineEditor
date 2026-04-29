#include "MenuBarPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/AnimationGraphPanel.h"
#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/ProjectSettingsPanel.h"
#include "UI/Panels/SaveScenePanel.h"
#include "UI/Panels/ShaderEditorPanel.h"
#include "UI/Panels/SystemFileBrowserPanel.h"

#include "Game.h"

#include "Goknar/Application.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/Helpers/SceneParser.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"

#include <filesystem>
#include <fstream>

#if GOKNAR_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
		#include <windows.h>
		#include <shellapi.h>
#else
	#include <unistd.h>
	#include <sys/types.h>
#endif

namespace
{
	std::string Trim(const std::string& value)
	{
		const size_t firstNonWhitespace = value.find_first_not_of(" \t\r\n");
		if (firstNonWhitespace == std::string::npos)
		{
			return "";
		}

		const size_t lastNonWhitespace = value.find_last_not_of(" \t\r\n");
		return value.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
	}

	std::string NormalizePath(const std::string& path)
	{
		std::string normalizedPath = path;
		for (char& character : normalizedPath)
		{
			if (character == '\\')
			{
				character = '/';
			}
		}

		return normalizedPath;
	}

	std::string EnsureTrailingSlash(const std::string& path)
	{
		std::string normalizedPath = NormalizePath(path);
		if (!normalizedPath.empty() && normalizedPath.back() != '/')
		{
			normalizedPath += '/';
		}

		return normalizedPath;
	}

	std::string GetProjectNameFromPath(const std::string& directoryPath)
	{
		std::filesystem::path projectPath(directoryPath);

		std::string projectName = projectPath.filename().string();
		if (projectName.empty())
		{
			projectName = projectPath.parent_path().filename().string();
		}

		return projectName;
	}

	std::string GetProjectBuildConfigPath(const std::string& directoryPath)
	{
		return EnsureTrailingSlash(directoryPath) + "Config/Build.ini";
	}

	std::string GetEngineLocationFromBuildConfig(const std::string& configPath)
	{
		std::ifstream buildConfig(configPath);
		if (!buildConfig.is_open())
		{
			return "";
		}

		std::string line;
		while (std::getline(buildConfig, line))
		{
			const size_t equalsIndex = line.find('=');
			if (equalsIndex == std::string::npos)
			{
				continue;
			}

			const std::string key = Trim(line.substr(0, equalsIndex));
			if (key != "EngineLocation")
			{
				continue;
			}

			return NormalizePath(Trim(line.substr(equalsIndex + 1)));
		}

		return "";
	}

	std::string GetFallbackEngineLocation()
	{
		const std::filesystem::path currentPath = std::filesystem::current_path();
		const std::filesystem::path candidatePaths[] =
		{
			currentPath / "GoknarEngine" / "Goknar",
			currentPath / ".." / "GoknarEngine" / "Goknar",
			currentPath / ".." / ".." / "GoknarEngine" / "Goknar"
		};

		for (const std::filesystem::path& candidatePath : candidatePaths)
		{
			std::error_code errorCode;
			const std::filesystem::path absolutePath = std::filesystem::weakly_canonical(candidatePath, errorCode);
			if (!errorCode && std::filesystem::exists(absolutePath))
			{
				return NormalizePath(absolutePath.string());
			}
		}

		return "";
	}

	std::string GetEditorEngineLocation()
	{
		std::string engineLocation = GetEngineLocationFromBuildConfig("Config/Build.ini");
		if (!engineLocation.empty())
		{
			return engineLocation;
		}

		return GetFallbackEngineLocation();
	}

	bool WriteBuildConfigFile(const std::string& configPath, const std::string& engineLocation, const std::string& projectName = "")
	{
		std::vector<std::pair<std::string, std::string>> entries;
		bool hasProjectName = projectName.empty();
		bool hasEngineLocation = false;

		{
			std::ifstream buildConfig(configPath);
			std::string line;
			while (std::getline(buildConfig, line))
			{
				const size_t equalsIndex = line.find('=');
				if (equalsIndex == std::string::npos)
				{
					continue;
				}

				const std::string key = Trim(line.substr(0, equalsIndex));
				const std::string value = Trim(line.substr(equalsIndex + 1));
				if (key.empty())
				{
					continue;
				}

				if (key == "ProjectName")
				{
					entries.emplace_back(key, projectName.empty() ? value : projectName);
					hasProjectName = true;
				}
				else if (key == "EngineLocation")
				{
					entries.emplace_back(key, NormalizePath(engineLocation));
					hasEngineLocation = true;
				}
				else
				{
					entries.emplace_back(key, value);
				}
			}
		}

		if (!hasProjectName && !projectName.empty())
		{
			entries.emplace_back("ProjectName", projectName);
		}

		if (!hasEngineLocation)
		{
			entries.emplace_back("EngineLocation", NormalizePath(engineLocation));
		}

		std::error_code errorCode;
		std::filesystem::create_directories(std::filesystem::path(configPath).parent_path(), errorCode);

		std::ofstream buildConfig(configPath);
		if (!buildConfig.is_open())
		{
			return false;
		}

		for (const std::pair<std::string, std::string>& entry : entries)
		{
			buildConfig << entry.first << "=" << entry.second << "\n";
		}

		return true;
	}

	bool WriteEditorConfigFile(const std::string& configPath, const std::string& currentProjectName = "", const std::string& currentProjectPath = "")
	{
		std::error_code errorCode;
		std::filesystem::create_directories(std::filesystem::path(configPath).parent_path(), errorCode);

		std::ofstream editorConfig(configPath);
		if (!editorConfig.is_open())
		{
			return false;
		}

		editorConfig << "[Editor]\n";
		if (!currentProjectName.empty())
		{
			editorConfig << "CurrentProject=" << currentProjectName << "\n";
		}

		if (!currentProjectPath.empty())
		{
			editorConfig << "CurrentProjectPath=" << EnsureTrailingSlash(currentProjectPath) << "\n";
		}

		return true;
	}
}

void MenuBarPanel::OnProjectSelected(const std::string& directoryPath)
{
	pendingProjectDirectoryPath_ = EnsureTrailingSlash(directoryPath);
	pendingProjectName_ = GetProjectNameFromPath(pendingProjectDirectoryPath_);
	editorEngineLocation_ = GetEditorEngineLocation();

	if (editorEngineLocation_.empty())
	{
		GOKNAR_CORE_WARN("EngineLocation could not be found in Config/Build.ini. Falling back to opening the project without updating its engine location.");
		ContinueOpeningProject();
		return;
	}

	const std::string projectConfigPath = GetProjectBuildConfigPath(pendingProjectDirectoryPath_);
	const std::string projectEngineLocation = GetEngineLocationFromBuildConfig(projectConfigPath);

	if (projectEngineLocation.empty())
	{
		if (!WriteBuildConfigFile(projectConfigPath, editorEngineLocation_, pendingProjectName_))
		{
			GOKNAR_CORE_ERROR("Failed to update %s with the selected engine location.", projectConfigPath.c_str());
			return;
		}

		ContinueOpeningProject();
		return;
	}

	if (NormalizePath(projectEngineLocation) != NormalizePath(editorEngineLocation_))
	{
		shouldOpenEngineLocationPopup_ = true;
		return;
	}

	ContinueOpeningProject();
}

void MenuBarPanel::ContinueOpeningProject()
{
	if (!WriteEditorConfigFile("Config/EditorConfig.ini", pendingProjectName_, pendingProjectDirectoryPath_))
	{
		GOKNAR_CORE_ERROR("Failed to update Config/EditorConfig.ini while opening the selected project.");
		return;
	}

#ifdef GOKNAR_PLATFORM_WINDOWS
	TCHAR szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, MAX_PATH);

	TCHAR szDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szDir);

	ShellExecute(NULL, NULL, szPath, NULL, szDir, SW_SHOWNORMAL);
#else
	pid_t pid = fork();

	if (pid == 0)
	{
		char* args[] = { (char*)"./GoknarEditor", NULL };
		execv(args[0], args);

		_exit(1);
	}
	else if (pid < 0)
	{
		GOKNAR_CORE_ERROR("Failed to fork process for restart.");
	}
#endif

	engine->Exit();
}

void MenuBarPanel::OnNewProjectSelected(const std::string& directoryPath, const std::string& projectName)
{
	std::string fullPath = directoryPath;
	if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\')
	{
		fullPath += '/';
	}
	fullPath += projectName;

	try
	{
		std::filesystem::copy("EditorData/NewProject", fullPath, std::filesystem::copy_options::recursive);

		std::string buildIniPath = fullPath + "/Config/Build.ini";
		editorEngineLocation_ = GetEditorEngineLocation();
		if (!WriteBuildConfigFile(buildIniPath, editorEngineLocation_, projectName))
		{
			GOKNAR_CORE_ERROR("Failed to write %s while creating the new project.", buildIniPath.c_str());
			return;
		}

		OnProjectSelected(fullPath);
	}
	catch (const std::exception& e)
	{
		GOKNAR_CORE_ERROR("Failed to create new project: %s", e.what());
	}
}

void MenuBarPanel::ReopenProjectSelector()
{
	SystemFileBrowserPanel* fileBrowser = static_cast<SystemFileBrowserPanel*>(hud_->GetPanel<SystemFileBrowserPanel>());
	fileBrowser->SetOnDirectorySelectedCallback(
		Delegate<void(const std::string&)>::Create<MenuBarPanel, &MenuBarPanel::OnProjectSelected>(this)
	);
	hud_->ShowPanel<SystemFileBrowserPanel>();
}

bool MenuBarPanel::SaveSceneToCurrentPath() const
{
	const std::string& path = EditorContext::Get()->sceneSavePath;
	if (path.empty())
	{
		hud_->ShowPanel<SaveScenePanel>();
		return false;
	}

	SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + path);
	return true;
}

void MenuBarPanel::SaveProject()
{
	if (!SaveSceneToCurrentPath())
	{
		return;
	}

	if (ShaderEditorPanel* shaderEditorPanel = hud_->GetPanel<ShaderEditorPanel>())
	{
		if (shaderEditorPanel->HasCurrentAssetPath())
		{
			shaderEditorPanel->SaveCurrentAsset();
		}
	}

	if (AnimationGraphPanel* animationGraphPanel = hud_->GetPanel<AnimationGraphPanel>())
	{
		if (animationGraphPanel->HasCurrentGraphPath())
		{
			animationGraphPanel->SaveCurrentAnimationGraph();
		}
	}

	AssetParser::SaveAssets("AssetContainer");
}

void MenuBarPanel::Draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Project"))
			{
				SystemFileBrowserPanel* fileBrowser = static_cast<SystemFileBrowserPanel*>(hud_->GetPanel<SystemFileBrowserPanel>());
				fileBrowser->CreateProjectSelector(
					Delegate<void(const std::string&, const std::string&)>::Create<MenuBarPanel, &MenuBarPanel::OnNewProjectSelected>(this)
				);
				hud_->ShowPanel<SystemFileBrowserPanel>();
			}

			if (ImGui::MenuItem("Open Project"))
			{
				SystemFileBrowserPanel* fileBrowser = static_cast<SystemFileBrowserPanel*>(hud_->GetPanel<SystemFileBrowserPanel>());
				fileBrowser->SetOnDirectorySelectedCallback(
					Delegate<void(const std::string&)>::Create<MenuBarPanel, &MenuBarPanel::OnProjectSelected>(this)
				);
				hud_->ShowPanel<SystemFileBrowserPanel>();
			}

			if (ImGui::MenuItem("Save"))
			{
				SaveProject();
			}

			if (ImGui::MenuItem("Save Scene As"))
			{
				hud_->ShowPanel<SaveScenePanel>();
			}

			if (ImGui::MenuItem("Save Scene"))
			{
				SaveSceneToCurrentPath();
			}

			if (ImGui::MenuItem("Exit"))
			{
				engine->Exit();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Project Settings"))
			{
				hud_->ShowPanel<ProjectSettingsPanel>();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Draw Collision Components"))
			{
				static bool drawCollisionWorld_ = false;
				drawCollisionWorld_ = !drawCollisionWorld_;
				if (drawCollisionWorld_)
				{
					const auto& objects = engine->GetObjectsOfType<RigidBody>();
					for (RigidBody* rigidBody : objects)
					{
						BoxCollisionComponent* boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(rigidBody->GetCollisionComponent());
						SphereCollisionComponent* sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(rigidBody->GetCollisionComponent());
						CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(rigidBody->GetCollisionComponent());
						MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(rigidBody->GetCollisionComponent());
						MultipleCollisionComponent* multipleCollisionComponent = dynamic_cast<MultipleCollisionComponent*>(rigidBody->GetCollisionComponent());

						if (boxCollisionComponent)
						{
							DebugDrawer::DrawCollisionComponent(boxCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (sphereCollisionComponent)
						{
							DebugDrawer::DrawCollisionComponent(sphereCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (capsuleCollisionComponent)
						{
							DebugDrawer::DrawCollisionComponent(capsuleCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (movingTriangleMeshCollisionComponent)
						{
							DebugDrawer::DrawCollisionComponent(movingTriangleMeshCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (multipleCollisionComponent)
						{
							for (const Component* component : multipleCollisionComponent->GetOwner()->GetComponents())
							{
								const BoxCollisionComponent* multiBoxCollisionComponent = dynamic_cast<const BoxCollisionComponent*>(component);
								const SphereCollisionComponent* multiSphereCollisionComponent = dynamic_cast<const SphereCollisionComponent*>(component);
								const CapsuleCollisionComponent* multiCapsuleCollisionComponent = dynamic_cast<const CapsuleCollisionComponent*>(component);
								const MovingTriangleMeshCollisionComponent* multiMovingTriangleMeshCollisionComponent = dynamic_cast<const MovingTriangleMeshCollisionComponent*>(component);

								if (multiBoxCollisionComponent)
								{
									DebugDrawer::DrawCollisionComponent(multiBoxCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (multiSphereCollisionComponent)
								{
									DebugDrawer::DrawCollisionComponent(multiSphereCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (multiCapsuleCollisionComponent)
								{
									DebugDrawer::DrawCollisionComponent(multiCapsuleCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (multiMovingTriangleMeshCollisionComponent)
								{
									DebugDrawer::DrawCollisionComponent(multiMovingTriangleMeshCollisionComponent, Colorf::Blue, 1.f);
								}
							}
						}
					}
				}
				else
				{
					const std::vector<DebugObject*>& objects = engine->GetObjectsOfType<DebugObject>();
					for (DebugObject* debugObject : objects)
					{
						debugObject->Destroy();
					}
				}
			}

			ViewportPanel* viewportPanel = static_cast<ViewportPanel*>(hud_->GetPanel<ViewportPanel>());
			if (viewportPanel)
			{
				static bool showDebugOverlay = viewportPanel->GetDebugOverlayEnabled();
				ImGui::MenuItem("Debug Overlay", nullptr, &showDebugOverlay);
				viewportPanel->SetDebugOverlayEnabled(showDebugOverlay);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			const std::vector<std::unique_ptr<IEditorPanel>>& panels = hud_->GetPanels();
			for (const std::unique_ptr<IEditorPanel>& panel : panels)
			{
				if (ImGui::MenuItem(((panel->GetIsOpen() ? "+ " : "- ") + panel->GetTitle()).c_str()))
				{
					panel->SetIsOpen(!panel->GetIsOpen());
				}
			}

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (shouldOpenEngineLocationPopup_)
	{
		ImGui::OpenPopup("Change Engine Location");
		shouldOpenEngineLocationPopup_ = false;
	}

	if (ImGui::BeginPopupModal("Change Engine Location", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("The selected game use a different engine location. In order to open it, the location need to be changed.");

		if (ImGui::Button("Open"))
		{
			const std::string projectConfigPath = GetProjectBuildConfigPath(pendingProjectDirectoryPath_);
			if (WriteBuildConfigFile(projectConfigPath, editorEngineLocation_, pendingProjectName_))
			{
				ImGui::CloseCurrentPopup();
				ContinueOpeningProject();
			}
			else
			{
				GOKNAR_CORE_ERROR("Failed to update %s with the editor engine location.", projectConfigPath.c_str());
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Don't open"))
		{
			ImGui::CloseCurrentPopup();
			ReopenProjectSelector();
		}

		ImGui::EndPopup();
	}
}
