#include "MenuBarPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/SaveScenePanel.h"
#include "UI/Panels/SystemFileBrowserPanel.h"

#include "Game.h"

#include "Goknar/Application.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Helpers/SceneParser.h"

#include <filesystem>
#include <fstream>

#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"

void MenuBarPanel::OnProjectSelected(const std::string& directoryPath)
{
	std::filesystem::path p(directoryPath);
	std::string projectName = p.filename().string();
	
	std::ofstream editorConfig("Config/EditorConfig.ini");
	if (editorConfig.is_open())
	{
		editorConfig << "[Editor]\n";
		editorConfig << "CurrentProject=" << projectName << "\n";
		editorConfig << "CurrentProjectPath=" << directoryPath << "/" << "\n";
		editorConfig.close();
	}

	((Game*)engine->GetApplication())->LoadProject(directoryPath);
}

void MenuBarPanel::Draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open Project"))
			{
				SystemFileBrowserPanel* fileBrowser = static_cast<SystemFileBrowserPanel*>(hud_->GetPanel<SystemFileBrowserPanel>());
				fileBrowser->SetOnDirectorySelectedCallback(
					Delegate<void(const std::string&)>::Create<MenuBarPanel, &MenuBarPanel::OnProjectSelected>(this)
				);
				hud_->ShowPanel<SystemFileBrowserPanel>();
			}

			if (ImGui::MenuItem("Save scene as"))
			{
				hud_->ShowPanel<SaveScenePanel>();
			}

			if (ImGui::MenuItem("Save scene"))
			{
				const std::string& path = EditorContext::Get()->sceneSavePath;

				if (path.empty())
				{
					hud_->ShowPanel<SaveScenePanel>();
				}
				else
				{
					SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + path);
				}
			}

			if (ImGui::MenuItem("Exit"))
			{
				engine->Exit();
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
						if (boxCollisionComponent)
						{
							DebugDrawer::DrawCollisionComponent(boxCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (SphereCollisionComponent* sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(rigidBody->GetCollisionComponent()))
						{
							DebugDrawer::DrawCollisionComponent(sphereCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(rigidBody->GetCollisionComponent()))
						{
							DebugDrawer::DrawCollisionComponent(capsuleCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(rigidBody->GetCollisionComponent()))
						{
							DebugDrawer::DrawCollisionComponent(movingTriangleMeshCollisionComponent, Colorf::Blue, 1.f);
						}
						else if (MultipleCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<MultipleCollisionComponent*>(rigidBody->GetCollisionComponent()))
						{
							for (const Component* component : movingTriangleMeshCollisionComponent->GetOwner()->GetComponents())
							{
								const BoxCollisionComponent* boxCollisionComponent = dynamic_cast<const BoxCollisionComponent*>(component);
								if (boxCollisionComponent)
								{
									DebugDrawer::DrawCollisionComponent(boxCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (const SphereCollisionComponent* sphereCollisionComponent = dynamic_cast<const SphereCollisionComponent*>(component))
								{
									DebugDrawer::DrawCollisionComponent(sphereCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (const CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<const CapsuleCollisionComponent*>(component))
								{
									DebugDrawer::DrawCollisionComponent(capsuleCollisionComponent, Colorf::Blue, 1.f);
								}
								else if (const MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<const MovingTriangleMeshCollisionComponent*>(component))
								{
									DebugDrawer::DrawCollisionComponent(movingTriangleMeshCollisionComponent, Colorf::Blue, 1.f);
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
}