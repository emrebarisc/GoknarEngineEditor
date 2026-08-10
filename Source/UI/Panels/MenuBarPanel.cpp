#include "MenuBarPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorConfigUtils.h"
#include "UI/EditorGameProjectBuildUtils.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/AnimationGraphPanel.h"
#include "UI/Panels/EditorSettingsPanel.h"
#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/ProjectSettingsPanel.h"
#include "UI/Panels/SaveScenePanel.h"
#include "UI/Panels/SystemFileBrowserPanel.h"
#include "UI/Panels/ShaderEditor/ShaderEditorPanel.h"
#ifdef GOKNAR_DEBUG
#include "UI/Panels/ProfilerPanel.h"
#endif

#include "Editor.h"

#include "Goknar/Application.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Helpers/AssetParser.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"

#include "UI/EditorSceneSerializer.h"

#include <filesystem>
#include <fstream>

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

	std::string ResolveEngineLocationFromConfigPath(const std::string& configPath, const std::string& engineLocation)
	{
		if (engineLocation.empty())
		{
			return "";
		}

		std::filesystem::path resolvedEnginePath(engineLocation);
		if (resolvedEnginePath.is_relative())
		{
			std::error_code errorCode;
			std::filesystem::path absoluteConfigPath = std::filesystem::absolute(std::filesystem::path(configPath), errorCode);
			if (errorCode)
			{
				absoluteConfigPath = std::filesystem::path(configPath);
			}

			const std::filesystem::path configDirectory = absoluteConfigPath.parent_path();
			const std::filesystem::path projectRoot = configDirectory.filename() == "Config"
				? configDirectory.parent_path()
				: configDirectory;
			resolvedEnginePath = projectRoot / resolvedEnginePath;
		}

		std::error_code errorCode;
		const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(resolvedEnginePath, errorCode);
		if (!errorCode)
		{
			return NormalizePath(canonicalPath.string());
		}

		return NormalizePath(resolvedEnginePath.lexically_normal().string());
	}

	bool IsEngineLocationValid(const std::string& engineLocation)
	{
		std::error_code errorCode;
		return std::filesystem::exists(std::filesystem::path(engineLocation) / "CMakeLists.txt", errorCode) && !errorCode;
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

	std::string QuoteCommandArgument(const std::string& value)
	{
		return "\"" + value + "\"";
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

	bool CopyDirectoryContents(const std::filesystem::path& sourceDirectory, const std::filesystem::path& destinationDirectory, bool overwriteExistingFiles)
	{
		std::error_code errorCode;
		if (!std::filesystem::is_directory(sourceDirectory, errorCode) || errorCode)
		{
			return true;
		}

		std::filesystem::create_directories(destinationDirectory, errorCode);
		if (errorCode)
		{
			GOKNAR_CORE_ERROR("Failed to create directory %s.", destinationDirectory.generic_string().c_str());
			return false;
		}

		for (const std::filesystem::directory_entry& directoryEntry : std::filesystem::recursive_directory_iterator(sourceDirectory, errorCode))
		{
			if (errorCode)
			{
				GOKNAR_CORE_ERROR("Failed to read directory %s.", sourceDirectory.generic_string().c_str());
				return false;
			}

			const std::filesystem::path relativePath = std::filesystem::relative(directoryEntry.path(), sourceDirectory, errorCode);
			if (errorCode)
			{
				GOKNAR_CORE_ERROR("Failed to resolve relative path for %s.", directoryEntry.path().generic_string().c_str());
				return false;
			}

			const std::filesystem::path destinationPath = destinationDirectory / relativePath;
			if (directoryEntry.is_directory(errorCode))
			{
				std::filesystem::create_directories(destinationPath, errorCode);
				if (errorCode)
				{
					GOKNAR_CORE_ERROR("Failed to create directory %s.", destinationPath.generic_string().c_str());
					return false;
				}
				continue;
			}

			if (!directoryEntry.is_regular_file(errorCode))
			{
				continue;
			}

			if (!overwriteExistingFiles && std::filesystem::exists(destinationPath, errorCode) && !errorCode)
			{
				continue;
			}

			std::filesystem::create_directories(destinationPath.parent_path(), errorCode);
			if (errorCode)
			{
				GOKNAR_CORE_ERROR("Failed to create directory %s.", destinationPath.parent_path().generic_string().c_str());
				return false;
			}

			const std::filesystem::copy_options copyOptions = overwriteExistingFiles ?
				std::filesystem::copy_options::overwrite_existing :
				std::filesystem::copy_options::none;
			std::filesystem::copy_file(directoryEntry.path(), destinationPath, copyOptions, errorCode);
			if (errorCode)
			{
				GOKNAR_CORE_ERROR("Failed to copy %s to %s.", directoryEntry.path().generic_string().c_str(), destinationPath.generic_string().c_str());
				return false;
			}
		}

		return true;
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

			return ResolveEngineLocationFromConfigPath(configPath, Trim(line.substr(equalsIndex + 1)));
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
			if (!errorCode && IsEngineLocationValid(absolutePath.string()))
			{
				return NormalizePath(absolutePath.string());
			}
		}

		return "";
	}

	std::string GetEditorEngineLocation()
	{
		std::string engineLocation = GetEngineLocationFromBuildConfig("Config/Build.ini");
		if (!engineLocation.empty() && IsEngineLocationValid(engineLocation))
		{
			return engineLocation;
		}

		return GetFallbackEngineLocation();
	}

	bool CopyPostProcessingEffectsToGameContent(const std::string& projectRootPath)
	{
		const std::filesystem::path projectRoot(projectRootPath);
		const std::filesystem::path destinationDirectory = projectRoot / "Content" / "Shaders" / "PostProcessing";

		std::string engineLocation = GetEngineLocationFromBuildConfig(GetProjectBuildConfigPath(projectRoot.generic_string()));
		if (engineLocation.empty() || !IsEngineLocationValid(engineLocation))
		{
			engineLocation = GetEditorEngineLocation();
		}

		if (!engineLocation.empty())
		{
			const std::filesystem::path enginePostProcessingDirectory = std::filesystem::path(engineLocation) / "EngineContent" / "Shaders" / "PostProcessing";
			if (!CopyDirectoryContents(enginePostProcessingDirectory, destinationDirectory, false))
			{
				return false;
			}
		}

		const std::filesystem::path projectEditorPostProcessingDirectory = projectRoot / "Editor" / "Shaders" / "PostProcessing";
		return CopyDirectoryContents(projectEditorPostProcessingDirectory, destinationDirectory, true);
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
		bool didSucceed = true;
		if (!currentProjectName.empty())
		{
			didSucceed &= EditorConfigUtils::SetEditorConfigValue(configPath, "Editor", "CurrentProject", currentProjectName);
		}

		if (!currentProjectPath.empty())
		{
			didSucceed &= EditorConfigUtils::SetEditorConfigValue(configPath, "Editor", "CurrentProjectPath", EnsureTrailingSlash(currentProjectPath));
		}

		return didSucceed;
	}

	void DrawPhysicsDebugMenu()
	{
		PhysicsWorld* physicsWorld = engine ? engine->GetPhysicsWorld() : nullptr;
		if (!physicsWorld)
		{
			return;
		}

		int debugMode = physicsWorld->GetPhysicsDebugMode();
		bool isPhysicsDebugEnabled = debugMode != btIDebugDraw::DBG_NoDebug;
		if (ImGui::MenuItem("Physics Debug World", nullptr, isPhysicsDebugEnabled))
		{
			isPhysicsDebugEnabled = !isPhysicsDebugEnabled;
			debugMode = isPhysicsDebugEnabled ?
				btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawContactPoints :
				btIDebugDraw::DBG_NoDebug;
			physicsWorld->SetPhysicsDebugMode(debugMode);
		}

		if (ImGui::BeginMenu("Physics Debug Options", isPhysicsDebugEnabled))
		{
			auto drawDebugModeFlag = [physicsWorld, &debugMode](const char* label, int flag)
				{
					const bool isEnabled = (debugMode & flag) != 0;
					if (ImGui::MenuItem(label, nullptr, isEnabled))
					{
						debugMode = isEnabled ? (debugMode & ~flag) : (debugMode | flag);
						physicsWorld->SetPhysicsDebugMode(debugMode);
					}
				};

			drawDebugModeFlag("Wireframe", btIDebugDraw::DBG_DrawWireframe);
			drawDebugModeFlag("AABBs", btIDebugDraw::DBG_DrawAabb);
			drawDebugModeFlag("Contact Points", btIDebugDraw::DBG_DrawContactPoints);
			drawDebugModeFlag("Constraints", btIDebugDraw::DBG_DrawConstraints);
			drawDebugModeFlag("Constraint Limits", btIDebugDraw::DBG_DrawConstraintLimits);
			drawDebugModeFlag("Normals", btIDebugDraw::DBG_DrawNormals);
			drawDebugModeFlag("Frames", btIDebugDraw::DBG_DrawFrames);

			ImGui::EndMenu();
		}
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
	if (!WriteEditorConfigFile(EditorGameProjectBuildUtils::GetEditorConfigPath(), pendingProjectName_, pendingProjectDirectoryPath_))
	{
		GOKNAR_CORE_ERROR("Failed to update Config/EditorConfig.ini while opening the selected project.");
		return;
	}

	const bool shouldRebuildGameEditorProject = !EditorGameProjectBuildUtils::IsCompiledGameEditorProjectCurrent(pendingProjectDirectoryPath_);
	if (!EditorGameProjectBuildUtils::RestartEditor(shouldRebuildGameEditorProject))
	{
		GOKNAR_CORE_ERROR(
			"Failed to restart editor%s.",
			shouldRebuildGameEditorProject ? " after starting game project rebuild" : "");
		return;
	}

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
		(void)e;
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

	hud_->PrepareSceneForSave();
	EditorSceneSerializer::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + path);
	EditorContext::Get()->ClearSceneDirty();
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

void MenuBarPanel::OnBuildDirectorySelected(const std::string& directoryPath)
{
	SaveProject();

	std::string projectRootPath = ProjectDir.empty() ? std::filesystem::current_path().string() : ProjectDir;
	projectRootPath = EnsureTrailingSlash(projectRootPath);

	const std::string normalizedProjectRootPath = projectRootPath.substr(0, projectRootPath.size() - 1);
	const std::string normalizedOutputPath = EnsureTrailingSlash(directoryPath);

	if (!CopyPostProcessingEffectsToGameContent(normalizedProjectRootPath))
	{
		GOKNAR_CORE_ERROR("Failed to copy post processing effects into the game content folder before publishing.");
		return;
	}

	std::string command;
#if GOKNAR_PLATFORM_WINDOWS
	command = "cd /d " + QuoteCommandArgument(normalizedProjectRootPath) + " && Build.sh publish " + QuoteCommandArgument("publishDir=" + normalizedOutputPath);
#else
	command = "cd " + QuoteCommandArgument(normalizedProjectRootPath) + " && ./Build.sh publish " + QuoteCommandArgument("publishDir=" + normalizedOutputPath);
#endif

	asyncBuildResult_ = std::async(std::launch::async, [command]()
		{
			std::system(command.c_str());
		});
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

			if (ImGui::MenuItem("Build the game"))
			{
				if (SystemFileBrowserPanel* fileBrowser = hud_->GetPanel<SystemFileBrowserPanel>())
				{
					fileBrowser->SetCurrentPath(ProjectDir.empty() ? std::filesystem::current_path().string() : ProjectDir);
					fileBrowser->OpenDirectorySelector(
						Delegate<void(const std::string&)>::Create<MenuBarPanel, &MenuBarPanel::OnBuildDirectorySelected>(this)
					);
				}
			}

			if (ImGui::MenuItem("Exit"))
			{
				engine->Exit();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Duplicate Object", "Ctrl+D", false, EditorContext::Get()->selectedObjectType == EditorSelectionType::Object))
			{
				hud_->CloneSelectedObjects();
			}

			if (ImGui::MenuItem("Project Settings"))
			{
				hud_->ShowPanel<ProjectSettingsPanel>();
			}

			if (ImGui::MenuItem("Editor Settings"))
			{
				hud_->ShowPanel<EditorSettingsPanel>();
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

			DrawPhysicsDebugMenu();

			ViewportPanel* viewportPanel = static_cast<ViewportPanel*>(hud_->GetPanel<ViewportPanel>());
			if (viewportPanel)
			{
				static bool showDebugOverlay = viewportPanel->GetDebugOverlayEnabled();
				ImGui::MenuItem("Debug Overlay", nullptr, &showDebugOverlay);
				viewportPanel->SetDebugOverlayEnabled(showDebugOverlay);
			}

#ifdef GOKNAR_DEBUG
			if (ImGui::MenuItem("Profiler"))
			{
				hud_->ShowPanel<ProfilerPanel>();
			}
#endif

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
