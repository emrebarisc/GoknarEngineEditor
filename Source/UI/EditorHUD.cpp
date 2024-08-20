#include "EditorHUD.h"

#include <string>
#include <unordered_map>

#include "imgui.h"
#include "imgui_internal.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"

#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"

#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"

#include "Goknar/Debug/DebugDrawer.h"

#include "Goknar/Factories/DynamicObjectFactory.h"

#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Goknar/Model/MeshUnit.h"

#include "Goknar/Physics/PhysicsDebugger.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"
#include "Goknar/Physics/Components/PhysicsMovementComponent.h"

#include "Goknar/Renderer/Texture.h"

#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "Goknar/Helpers/SceneParser.h"

#include "Game.h"
#include "Objects/FreeCameraObject.h"
#include "Thirdparty/ImGuiOpenGL.h"

template <class T>
void AddCollisionComponent(PhysicsObject* physicsObject)
{
	if (physicsObject->GetFirstComponentOfType<CollisionComponent>())
	{
		return;
	}

	T* collisionComponent = physicsObject->AddSubComponent<T>();
}

EditorHUD::EditorHUD() : HUD()
{
    SetIsTickable(true);
	
	imguiContext_ = ImGui::CreateContext();

	engine->SetHUD(this);

	onKeyboardEventDelegate_ = Delegate<void(int, int, int, int)>::create<EditorHUD, &EditorHUD::OnKeyboardEvent>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::create<EditorHUD, &EditorHUD::OnCursorMove>(this);
	onScrollDelegate_ = Delegate<void(double, double)>::create<EditorHUD, &EditorHUD::OnScroll>(this);
	onLeftClickPressedDelegate_ = Delegate<void()>::create<EditorHUD, &EditorHUD::OnLeftClickPressed>(this);
	onLeftClickReleasedDelegate_ = Delegate<void()>::create<EditorHUD, &EditorHUD::OnLeftClickReleased>(this);
	onCharPressedDelegate_ = Delegate<void(unsigned int)>::create<EditorHUD, &EditorHUD::OnCharPressed>(this);
	onWindowSizeChangedDelegate_ = Delegate<void(int, int)>::create<EditorHUD, &EditorHUD::OnWindowSizeChanged>(this);
	onDeleteInputPressedDelegate_ = Delegate<void()>::create<EditorHUD, &EditorHUD::OnDeleteInputPressed>(this);
	onFocusInputPressedDelegate_ = Delegate<void()>::create<EditorHUD, &EditorHUD::OnFocusInputPressed>(this);
	onCancelInputPressedDelegate_ = Delegate<void()>::create<EditorHUD, &EditorHUD::OnCancelInputPressed>(this);

	uiImage_ = engine->GetResourceManager()->GetContent<Image>("Textures/UITexture.png");

	BuildFileTree();

	objectBaseReflections["StaticMeshComponent"] = 
		[](ObjectBase* objectBase) 
		{
			objectBase->AddSubComponent<StaticMeshComponent>();
		};

	physicsObjectReflections["BoxCollisionComponent"] = 
		[this](PhysicsObject* physicsObject)
		{ 
			AddCollisionComponent<BoxCollisionComponent>(physicsObject);
		};
	physicsObjectReflections["CapsuleCollisionComponent"] = 
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<CapsuleCollisionComponent>(physicsObject);
		};
	physicsObjectReflections["SphereCollisionComponent"] = 
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<SphereCollisionComponent>(physicsObject);
		};
	physicsObjectReflections["MovingTriangleMeshCollisionComponent"] = 
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<MovingTriangleMeshCollisionComponent>(physicsObject);
		};
	physicsObjectReflections["NonMovingTriangleMeshCollisionComponent"] =
		[](PhysicsObject* physicsObject)
		{
			AddCollisionComponent<NonMovingTriangleMeshCollisionComponent>(physicsObject);
		};

	windowOpenMap_[gameOptionsWindowName_] = true;
	windowOpenMap_[sceneWindowName_] = true;
	windowOpenMap_[objectsWindowName_] = true;
	windowOpenMap_[fileBrowserWindowName_] = true;
	windowOpenMap_[detailsWindowName_] = true;
}

EditorHUD::~EditorHUD()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->RemoveKeyboardListener(onKeyboardEventDelegate_);

	inputManager->RemoveCursorDelegate(onCursorMoveDelegate_);
	inputManager->RemoveScrollDelegate(onScrollDelegate_);

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_PRESS, onLeftClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_RELEASE, onLeftClickReleasedDelegate_);

	inputManager->RemoveCharDelegate(onCharPressedDelegate_);

	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::DLT, INPUT_ACTION::G_PRESS, onDeleteInputPressedDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::F, INPUT_ACTION::G_PRESS, onFocusInputPressedDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::ESCAPE, INPUT_ACTION::G_PRESS, onCancelInputPressedDelegate_);

	ImGui_DestroyFontsTexture();
	ImGui_DestroyDeviceObjects();
	ImGui_Shutdown();
	ImGui::DestroyContext(imguiContext_);

	for (auto folder : folderMap)
	{
		delete folder.second;
	}

	delete rootFolder;
}

void EditorHUD::PreInit()
{
	HUD::PreInit();

	InputManager* inputManager = engine->GetInputManager();

	inputManager->AddKeyboardListener(onKeyboardEventDelegate_);

	inputManager->AddCursorDelegate(onCursorMoveDelegate_);
	inputManager->AddScrollDelegate(onScrollDelegate_);

	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_PRESS, onLeftClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_RELEASE, onLeftClickReleasedDelegate_);

	inputManager->AddCharDelegate(onCharPressedDelegate_);

	inputManager->AddKeyboardInputDelegate(KEY_MAP::DLT, INPUT_ACTION::G_PRESS, onDeleteInputPressedDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::F, INPUT_ACTION::G_PRESS, onFocusInputPressedDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::ESCAPE, INPUT_ACTION::G_PRESS, onCancelInputPressedDelegate_);
}

void EditorHUD::Init()
{
	HUD::Init();
}

void EditorHUD::PostInit()
{
	HUD::PostInit();
}

void EditorHUD::BeginGame()
{
    HUD::BeginGame();
	
    WindowManager* windowManager = engine->GetWindowManager();
	windowSize_ = windowManager->GetWindowSize();

	Vector2 buttonSizeVector = windowSize_ * 0.05f;
	buttonSize_ = ImVec2(buttonSizeVector.x, buttonSizeVector.y);

	windowManager->AddWindowSizeCallback(onWindowSizeChangedDelegate_);
	
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)windowSize_.x, (float)windowSize_.y);
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	io.KeyRepeatRate = 0.1f;

	ImGui_Init("#version 410 core");

	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);
	colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.f);
}

void EditorHUD::OnKeyboardEvent(int key, int scanCode, int action, int mod)
{
	ImGuiIO &io = ImGui::GetIO();

	switch (action)
	{
	case GLFW_PRESS:
	{
		io.KeysDown[key] = true;

		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
		break;
	}
	case GLFW_RELEASE:
	{
		io.KeysDown[key] = false;
		break;
	}
	default:
		break;
	}
}

void EditorHUD::OnCursorMove(double xPosition, double yPosition)
{
	ImGuiIO &io = ImGui::GetIO();
	io.MousePos.x = (float)xPosition;
	io.MousePos.y = (float)yPosition;
}

void EditorHUD::OnScroll(double xOffset, double yOffset)
{
	ImGuiIO &io = ImGui::GetIO();
	io.MouseWheelH += (float)xOffset;
	io.MouseWheel += (float)yOffset;
}

void EditorHUD::OnLeftClickPressed()
{
	ImGuiIO &io = ImGui::GetIO();
	io.MouseDown[GLFW_MOUSE_BUTTON_1] = true;

	if (objectToCreateType_ != Editor_ObjectType::None)
	{
		OnPlaceObject();
	}
}

void EditorHUD::OnLeftClickReleased()
{
	ImGuiIO &io = ImGui::GetIO();
	io.MouseDown[GLFW_MOUSE_BUTTON_1] = false;
}

void EditorHUD::OnCharPressed(unsigned int codePoint)
{
	ImGuiIO &io = ImGui::GetIO();
	io.AddInputCharacter(codePoint);
}

void EditorHUD::OnWindowSizeChanged(int width, int height)
{
	windowSize_ = Vector2i(width, height);
	Vector2 buttonSizeVector = windowSize_ * 0.05f;
	buttonSize_ = ImVec2(buttonSizeVector.x, buttonSizeVector.y);
}

void EditorHUD::OnPlaceObject()
{
	Vector3 raycastPosition = RaycastWorld();

	switch (objectToCreateType_)
	{
	case Editor_ObjectType::Object:
	{
		ObjectBase* object = CreateObject(objectToCreateName_);
		if (object)
		{
			object->SetWorldPosition(raycastPosition);
		}
		break;
	}
	case Editor_ObjectType::DirectionalLight:
	{
		CreateDirectionalLight();
		break;
	}
	case Editor_ObjectType::PointLight:
	{
		PointLight* pointLight = CreatePointLight();
		pointLight->SetPosition(raycastPosition);
		break;
	}
	case Editor_ObjectType::SpotLight:
	{
		CreateSpotLight();
		SpotLight* spotLight = CreateSpotLight();
		spotLight->SetPosition(raycastPosition);
		break;
	}
	case Editor_ObjectType::None:
	default:
		break;
	}

	objectToCreateType_ = Editor_ObjectType::None;
	objectToCreateName_ = "";
}

Vector3 EditorHUD::RaycastWorld()
{
	Camera* activeCamera = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject()->GetCameraComponent()->GetCamera();

	InputManager* inputManager = engine->GetInputManager();

	double x, y;
	inputManager->GetCursorPosition(x, y);

	Vector3 cameraWorldPosition = activeCamera->GetPosition();
	Vector3 cameraForwardVector = activeCamera->GetWorldDirectionAtPixel(Vector2i{ (int)x, (int)y });

	RaycastData raycastData;
	raycastData.from = cameraWorldPosition;
	raycastData.to = cameraWorldPosition + cameraForwardVector * 1000.f;
	raycastData.collisionGroup = CollisionGroup::All;
	raycastData.collisionMask = CollisionMask(0xFFFFFF);

	RaycastSingleResult raycastResult;

	engine->GetPhysicsWorld()->RaycastClosest(raycastData, raycastResult);

	return raycastResult.hitPosition;
}

void EditorHUD::OnDeleteInputPressed()
{
	switch (selectedObjectType_)
	{
	case Editor_ObjectType::Object:
		((ObjectBase*)selectedObject_)->Destroy();
		break;
	case Editor_ObjectType::DirectionalLight:
		delete ((DirectionalLight*)selectedObject_);
		break;
	case Editor_ObjectType::PointLight:
		delete ((PointLight*)selectedObject_);
		break;
	case Editor_ObjectType::SpotLight:
		delete ((SpotLight*)selectedObject_);
		break;
	case Editor_ObjectType::None:
	default:
		return;
	}

	selectedObjectType_ = Editor_ObjectType::None;
	selectedObject_ = nullptr;
}

void EditorHUD::OnFocusInputPressed()
{
	Vector3 position = Vector3::ZeroVector;
	switch (selectedObjectType_)
	{
	case Editor_ObjectType::Object:
		position = ((ObjectBase*)selectedObject_)->GetWorldPosition();
		break;
	case Editor_ObjectType::DirectionalLight:
	case Editor_ObjectType::PointLight:
	case Editor_ObjectType::SpotLight:
		position = ((Light*)selectedObject_)->GetPosition();
		break;
	case Editor_ObjectType::None:
	default:
		return;
	}

	FocusToPosition(position);
}

void EditorHUD::OnCancelInputPressed()
{
	if (objectToCreateType_ != Editor_ObjectType::None)
	{
		objectToCreateType_ = Editor_ObjectType::None;
		objectToCreateName_ = "";
	}
}

void EditorHUD::UpdateHUD()
{
	HUD::UpdateHUD();

    WindowManager* windowManager = engine->GetWindowManager();
	windowSize_ = windowManager->GetWindowSize();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.DisplaySize = ImVec2((float)windowSize_.x, (float)windowSize_.y);

	ImGui_NewFrame();
	ImGui::NewFrame();

	ImGui::StyleColorsDark();

	DrawEditorHUD();

	ImGui::Render();
	ImGui_RenderDrawData(ImGui::GetDrawData());
}

void EditorHUD::DrawEditorHUD()
{
	static bool p_open = true;
	static bool opt_fullscreen = true;
	static bool opt_padding = false;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	else
	{
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
	}

	// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
	// and handle the pass-thru hole, so we ask Begin() to not render a background.
	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
	// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
	if (!opt_padding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Goknar Engine Editor", &p_open, window_flags);
	if (!opt_padding)
		ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	// Submit the DockSpace
	ImGuiIO& io = ImGui::GetIO(); 

	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("Editor");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}
	else
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::Text("ERROR: Docking is not enabled! See Demo > Configuration.");
		ImGui::Text("Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code, or ");
		ImGui::SameLine(0.0f, 0.0f);
		if (ImGui::SmallButton("click here"))
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save scene as"))
			{
				windowOpenMap_[saveSceneDialogWindowName_] = true;
			}

			if (ImGui::MenuItem("Save scene"))
			{
				if (sceneSavePath_.empty())
				{
					windowOpenMap_[saveSceneDialogWindowName_] = true;
				}
				else
				{
					SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + sceneSavePath_);
				}
			}

			if (ImGui::MenuItem("Exit", NULL, false, p_open != NULL))
			{
				engine->Exit();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Draw Collision Components"))
			{
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
					const auto& objects = engine->GetObjectsOfType<DebugObject>();
					for (DebugObject* debugObject : objects)
					{
						debugObject->Destroy();
					}
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem((std::string(windowOpenMap_[gameOptionsWindowName_] ? "+ " : "- ") + "Game Options").c_str()))
			{
				windowOpenMap_[gameOptionsWindowName_] = !windowOpenMap_[gameOptionsWindowName_];
			}
			if (ImGui::MenuItem((std::string(windowOpenMap_[sceneWindowName_] ? "+ " : "- ") + "Scene Window").c_str()))
			{
				windowOpenMap_[sceneWindowName_] = !windowOpenMap_[sceneWindowName_];
			}
			if (ImGui::MenuItem((std::string(windowOpenMap_[objectsWindowName_] ? "+ " : "- ") + "Objects").c_str()))
			{
				windowOpenMap_[objectsWindowName_] = !windowOpenMap_[objectsWindowName_];
			}
			if (ImGui::MenuItem((std::string(windowOpenMap_[fileBrowserWindowName_] ? "+ " : "- ") + "File Browser").c_str()))
			{
				windowOpenMap_[fileBrowserWindowName_] = !windowOpenMap_[fileBrowserWindowName_];
			}
			if (ImGui::MenuItem((std::string(windowOpenMap_[detailsWindowName_] ? "+ " : "- ") + "Details").c_str()))
			{
				windowOpenMap_[detailsWindowName_] = !windowOpenMap_[detailsWindowName_];
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options"))
		{
			// Disabling fullscreen would allow the window to be moved to the front of other windows,
			// which we can't undo at the moment without finer window depth/z control.
			ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
			ImGui::MenuItem("Padding", NULL, &opt_padding);
			ImGui::Separator();

			if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode; }
			if (ImGui::MenuItem("Flag: NoDockingSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingSplit) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit; }
			if (ImGui::MenuItem("Flag: NoUndocking", "", (dockspace_flags & ImGuiDockNodeFlags_NoUndocking) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking; }
			if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
			if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
			if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
			ImGui::Separator();

			if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
				p_open = false;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::End();

	DrawCameraInfo();

	if (objectToCreateType_ != Editor_ObjectType::None)
	{
		DrawObjectNameToCreateWindow();
	}
	
	if (windowOpenMap_[sceneWindowName_])
	{
		DrawSceneWindow();
	}

	if (windowOpenMap_[objectsWindowName_])
	{
		DrawObjectsWindow();
	}

	if (windowOpenMap_[fileBrowserWindowName_])
	{
		DrawFileBrowserWindow();
	}

	if (windowOpenMap_[detailsWindowName_])
	{
		DrawDetailsWindow();
	}
	
	if (windowOpenMap_[gameOptionsWindowName_])
	{
		DrawGameOptionsBar();
	}

	if (windowOpenMap_[saveSceneDialogWindowName_])
	{
		OpenSaveSceneDialog();
	}
}

void EditorHUD::DrawCameraInfo()
{
	BeginTransparentWindow("CameraInfo");

	FreeCameraObject* freeCameraObject = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject();
	ImGui::Text((std::string("Position: ") + freeCameraObject->GetWorldPosition().ToString()).c_str());
	ImGui::Text((std::string("Rotation: ") + freeCameraObject->GetWorldRotation().ToEulerDegrees().ToString()).c_str());
	ImGui::Text((std::string("Forward Vector: ") + freeCameraObject->GetForwardVector().ToString()).c_str());
	ImGui::Text((std::string("Left Vector: ") + freeCameraObject->GetLeftVector().ToString()).c_str());
	ImGui::Text((std::string("Up Vector: ") + freeCameraObject->GetUpVector().ToString()).c_str());

	EndWindow();
}

void EditorHUD::DrawSceneWindow()
{
	BeginWindow(sceneWindowName_, dockableWindowFlags_);
	DrawSceneLights();
	DrawSceneObjects();
	EndWindow();
}

void EditorHUD::DrawSceneLights()
{
	if (ImGui::TreeNode("Lights"))
	{
		Scene* currentScene = engine->GetApplication()->GetMainScene();

		std::vector<DirectionalLight*> directionalLights = currentScene->GetDirectionalLights();
		int directionalLightCount = directionalLights.size();

		std::vector<PointLight*> pointLights = currentScene->GetPointLights();
		int pointLightCount = pointLights.size();

		std::vector<SpotLight*> spotLights = currentScene->GetSpotLights();
		int spotLightCount = spotLights.size();

		if (0 < directionalLightCount)
		{
			for (int lightIndex = 0; lightIndex < directionalLightCount; lightIndex++)
			{
				DirectionalLight* directionalLight = directionalLights[lightIndex];

				if (ImGui::Selectable(directionalLight->GetName().c_str(), selectedObject_ == directionalLight, ImGuiSelectableFlags_AllowDoubleClick))
				{
					selectedObject_ = directionalLight;
					selectedObjectType_ = Editor_ObjectType::DirectionalLight;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
					{
					}
				}
			}
		}
		if (0 < pointLightCount)
		{
			for (int lightIndex = 0; lightIndex < pointLightCount; lightIndex++)
			{
				PointLight* pointLight = pointLights[lightIndex];

				if (ImGui::Selectable(pointLight->GetName().c_str(), selectedObject_ == pointLight, ImGuiSelectableFlags_AllowDoubleClick))
				{
					selectedObject_ = pointLight;
					selectedObjectType_ = Editor_ObjectType::PointLight;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
					{
						FocusToPosition(pointLight->GetPosition());
					}
				}
			}
		}
		if (0 < spotLightCount)
		{
			for (int lightIndex = 0; lightIndex < spotLightCount; lightIndex++)
			{
				SpotLight* spotLight = spotLights[lightIndex];

				if (ImGui::Selectable(spotLight->GetName().c_str(), selectedObject_ == spotLight, ImGuiSelectableFlags_AllowDoubleClick))
				{
					selectedObject_ = spotLight;
					selectedObjectType_ = Editor_ObjectType::SpotLight;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
					{
						FocusToPosition(spotLight->GetPosition());
					}
				}
			}
		}

		ImGui::TreePop();
	}
}

void EditorHUD::DrawSceneObjects()
{
	if (ImGui::TreeNode("Objects"))
	{
		std::vector<ObjectBase*> registeredObjects = engine->GetObjectsOfType<ObjectBase>();

		for (ObjectBase* object : registeredObjects)
		{
			if (object->GetParent() != nullptr)
			{
				continue;
			}

			DrawSceneObject(object);
		}

		ImGui::TreePop();
	}
}

void EditorHUD::DrawSceneObject(ObjectBase* object)
{
	if (object->GetName().find("__Editor__") != std::string::npos)
	{
		return;
	}

	if (ImGui::Selectable(object->GetName().c_str(), selectedObject_ == object, ImGuiSelectableFlags_AllowDoubleClick))
	{
		selectedObject_ = object;
		selectedObjectType_ = Editor_ObjectType::Object;

		if (ImGui::IsMouseDoubleClicked(ImGuiButtonFlags_MouseButtonLeft))
		{
			FreeCameraObject* freeCameraObject = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject();
			freeCameraObject->SetWorldPosition(object->GetWorldPosition() - 20.f * freeCameraObject->GetForwardVector());
		}
	}

	const std::vector<ObjectBase*>& children = object->GetChildren();
	int childrenCount = children.size();
	
	for (int childIndex = 0; childIndex < childrenCount; ++childIndex)
	{
		ObjectBase* child = children[childIndex];
		const std::vector<ObjectBase*>& childrenOfChild = child->GetChildren();
		int childrenOfChildCount = childrenOfChild.size();

		if (0 == childrenOfChildCount)
		{
			DrawSceneObject(child);
		}
		else
		{
			if (ImGui::TreeNode(child->GetName().c_str()))
			{
				DrawSceneObject(child);

				ImGui::TreePop();
			}
		}
	}
}

void EditorHUD::DrawFileTree(Folder* folder)
{
	for (auto subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->folderName.c_str()))
		{
			DrawFileTree(subFolder);
			ImGui::TreePop();
		}
	}

	for (auto fileName : folder->fileNames)
	{
		ImGui::Text(fileName.c_str());
	}
}

void EditorHUD::DrawFileGrid(Folder* folder, std::string& selectedFileName, bool& isAFileSelected)
{
	for (auto subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->folderName.c_str()))
		{
			DrawFileGrid(subFolder, selectedFileName, isAFileSelected);
			ImGui::TreePop();
		}
	}

	ImGui::Columns(4, nullptr, false);
	int fileCount = folder->fileNames.size();
	for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		std::string fileName = folder->fileNames[fileIndex];

		if (ImGui::Button(fileName.c_str(), { 150.f, 30.f }))
		{
			selectedFileName = folder->fullPath + fileName;
			isAFileSelected = true;
		}

		ImGui::NextColumn();
	}
	ImGui::Columns(1, nullptr, false);
}

void EditorHUD::DrawObjectsWindow()
{
	BeginWindow(objectsWindowName_, dockableWindowFlags_);
	if (ImGui::TreeNode("Lights"))
	{
		if (ImGui::Button("Directional Light"))
		{
			objectToCreateType_ = Editor_ObjectType::DirectionalLight;
			objectToCreateName_ = "Directional Light";
		}
		else if (ImGui::Button("Point Light"))
		{
			objectToCreateType_ = Editor_ObjectType::PointLight;
			objectToCreateName_ = "Point Light";
		}
		else if (ImGui::Button("Spot Light"))
		{
			objectToCreateType_ = Editor_ObjectType::SpotLight;
			objectToCreateName_ = "Spot Light";
		}

		ImGui::TreePop();
	}
	ImGui::Separator();
	if (ImGui::TreeNode("Objects"))
	{
		static const auto& objects = DynamicObjectFactory::GetInstance()->GetObjectMap();

		for (auto object : objects)
		{
			std::string name = object.first;
			if (ImGui::Button(name.c_str()))
			{
				objectToCreateType_ = Editor_ObjectType::Object;
				objectToCreateName_ = name;
			}
		}
		
		ImGui::TreePop();
	}
	ImGui::Separator();
	EndWindow();
}

void EditorHUD::BuildFileTree()
{
	std::vector<std::string> contentPaths;

	ResourceManager* resourceManager = engine->GetResourceManager();

	int resourceIndex = 0;
	while (MeshUnit* mesh = resourceManager->GetResourceContainer()->GetMesh(resourceIndex))
	{
#ifdef ENGINE_CONTENT_DIR
		if (mesh->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = mesh->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
#endif
		++resourceIndex;
	}

	resourceIndex = 0;
	while (Image* image = resourceManager->GetResourceContainer()->GetImage(resourceIndex))
	{
#ifdef ENGINE_CONTENT_DIR
		if (image->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = image->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
#endif
		++resourceIndex;
	}

	for (std::string& contentPath : contentPaths)
	{
		for (char& c : contentPath)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}
	}

	resourceIndex = 0;
	while (Audio* audio = resourceManager->GetResourceContainer()->GetAudio(resourceIndex))
	{
#ifdef ENGINE_CONTENT_DIR
		if (audio->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = audio->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
#endif
		++resourceIndex;
	}

	std::sort(contentPaths.begin(), contentPaths.end());

	rootFolder = new Folder();
	rootFolder->folderName = "Content";

	int contentPathsSize = contentPaths.size();
	for (size_t pathIndex = 0; pathIndex < contentPathsSize; pathIndex++)
	{
		std::string currentPath = contentPaths[pathIndex];
		std::string fullPath = currentPath;

		Folder* parentFolder = rootFolder;

		bool breakTheLoop = false;
		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));
			std::string folderPath = fullPath.substr(0, fullPath.find(folderName) + folderName.size() + 1);

			if (folderName.find('.') != std::string::npos)
			{
				if (parentFolder)
				{
					parentFolder->fileNames.push_back(folderName);
				}

				breakTheLoop = true;
			}
			else
			{
				if (folderMap.find(folderPath) == folderMap.end())
				{
					folderMap[folderPath] = new Folder();
					folderMap[folderPath]->folderName = folderName;
					folderMap[folderPath]->fullPath = folderPath;
				}

				if (std::find(parentFolder->subFolders.begin(),
					parentFolder->subFolders.end(),
					folderMap[folderPath]) == std::end(parentFolder->subFolders))
				{
					parentFolder->subFolders.push_back(folderMap[folderPath]);
				}
			}

			if (breakTheLoop)
			{
				break;
			}

			parentFolder = folderMap[folderPath];
			currentPath = currentPath.substr(folderName.size() + 1);
		}
	}
}

void EditorHUD::DrawFileBrowserWindow()
{
	BeginWindow(fileBrowserWindowName_, dockableWindowFlags_);

	if (ImGui::TreeNode("Assets"))
	{
		DrawFileTree(rootFolder);
		ImGui::TreePop();
	}

	EndWindow();
}

void EditorHUD::DrawDetailsWindow()
{
	BeginWindow(detailsWindowName_, dockableWindowFlags_);

	switch (selectedObjectType_)
	{
	case Editor_ObjectType::None:
		break;
	case Editor_ObjectType::Object:
		DrawDetailsWindow_Object();
		break;
	case Editor_ObjectType::DirectionalLight:
		DrawDetailsWindow_DirectionalLight();
		break;
	case Editor_ObjectType::PointLight:
		DrawDetailsWindow_PointLight();
		break;
	case Editor_ObjectType::SpotLight:
		DrawDetailsWindow_SpotLight();
		break;
	default:
		break;
	}

	EndWindow();
}

void EditorHUD::DrawDetailsWindow_Object()
{
	ObjectBase* selectedObject = static_cast<ObjectBase*>(selectedObject_);

	Vector3 selectedObjectWorldPosition = selectedObject->GetWorldPosition();
	Vector3 selectedObjectWorldRotationEulerDegrees = selectedObject->GetWorldRotation().ToEulerDegrees();
	Vector3 selectedObjectWorldScaling = selectedObject->GetWorldScaling();
	std::string selectedObjectName = selectedObject->GetNameWithoutId();

	ImGui::Text(selectedObjectName.c_str());
	ImGui::Separator();

	ImGui::Text("Add Component: ");
	ImGui::SameLine();

	DrawDetailsWindow_AddComponentOptions(selectedObject);

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Name: ");
	ImGui::SameLine();
	std::string newName = selectedObjectName;
	DrawInputText("##Name", newName);
	if (newName != selectedObjectName)
	{
		selectedObject->SetName(newName);
	}

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputFloat("##Position", selectedObjectWorldPosition);
	selectedObject->SetWorldPosition(selectedObjectWorldPosition);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	DrawInputFloat("##Rotation", selectedObjectWorldRotationEulerDegrees);
	selectedObject->SetWorldRotation(Quaternion::FromEulerDegrees(selectedObjectWorldRotationEulerDegrees));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	DrawInputFloat("##Scaling", selectedObjectWorldScaling);
	selectedObject->SetWorldScaling(selectedObjectWorldScaling);

	ImGui::Separator();
	RigidBody* rigidBody = dynamic_cast<RigidBody*>(selectedObject);
	if (rigidBody)
	{
		float rigidBodyMass = rigidBody->GetMass();
		ImGui::Text("Mass: ");
		ImGui::SameLine();
		DrawInputFloat("##Mass", rigidBodyMass);
		rigidBody->SetMass(rigidBodyMass);

		static const char* collisionGroupNames[]{
			"",
			"Default",
			"WorldStaticBlock",
			"WorldDynamicBlock",
			"AllBlock",
			"WorldDynamicOverlap",
			"WorldStaticOverlap",
			"AllOverlap",
			"Character",
			"All",
			"Custom0",
			"Custom1",
			"Custom2",
			"Custom3",
			"Custom4",
			"Custom5",
			"Custom6",
			"Custom7",
			"Custom8",
			"Custom9"
		};

		int selectedCollisionGroup = 1;
		CollisionGroup collisionGroup = rigidBody->GetCollisionGroup();
		if (collisionGroup == CollisionGroup::Default) selectedCollisionGroup = 1;
		else if (collisionGroup == CollisionGroup::WorldStaticBlock) selectedCollisionGroup = 2;
		else if (collisionGroup == CollisionGroup::WorldDynamicBlock) selectedCollisionGroup = 3;
		else if (collisionGroup == CollisionGroup::AllBlock) selectedCollisionGroup = 4;
		else if (collisionGroup == CollisionGroup::WorldDynamicOverlap) selectedCollisionGroup = 5;
		else if (collisionGroup == CollisionGroup::WorldStaticOverlap) selectedCollisionGroup = 6;
		else if (collisionGroup == CollisionGroup::AllOverlap) selectedCollisionGroup = 7;
		else if (collisionGroup == CollisionGroup::Character) selectedCollisionGroup = 8;
		else if (collisionGroup == CollisionGroup::All) selectedCollisionGroup = 9;
		else if (collisionGroup == CollisionGroup::Custom0) selectedCollisionGroup = 10;
		else if (collisionGroup == CollisionGroup::Custom1) selectedCollisionGroup = 11;
		else if (collisionGroup == CollisionGroup::Custom2) selectedCollisionGroup = 12;
		else if (collisionGroup == CollisionGroup::Custom3) selectedCollisionGroup = 13;
		else if (collisionGroup == CollisionGroup::Custom4) selectedCollisionGroup = 14;
		else if (collisionGroup == CollisionGroup::Custom5) selectedCollisionGroup = 15;
		else if (collisionGroup == CollisionGroup::Custom6) selectedCollisionGroup = 16;
		else if (collisionGroup == CollisionGroup::Custom7) selectedCollisionGroup = 17;
		else if (collisionGroup == CollisionGroup::Custom8) selectedCollisionGroup = 18;
		else if (collisionGroup == CollisionGroup::Custom9) selectedCollisionGroup = 19;

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, 125);
		ImGui::Text("CollisionGroup: ");
		ImGui::NextColumn();
		ImGui::SetColumnWidth(1, 800);
		bool collisionGroupCheck = ImGui::Combo("##CollisionGroup", &selectedCollisionGroup, collisionGroupNames, IM_ARRAYSIZE(collisionGroupNames));
		if (collisionGroupCheck)
		{
			CollisionGroup collisionGroup = CollisionGroup::Default;
			if (selectedCollisionGroup == 2)
			{
				collisionGroup = CollisionGroup::WorldStaticBlock;
			}
			else if (selectedCollisionGroup == 3)
			{
				collisionGroup = CollisionGroup::WorldDynamicBlock;
			}
			else if (selectedCollisionGroup == 4)
			{
				collisionGroup = CollisionGroup::AllBlock;
			}
			else if (selectedCollisionGroup == 5)
			{
				collisionGroup = CollisionGroup::WorldDynamicOverlap;
			}
			else if (selectedCollisionGroup == 6)
			{
				collisionGroup = CollisionGroup::WorldStaticOverlap;
			}
			else if (selectedCollisionGroup == 7)
			{
				collisionGroup = CollisionGroup::AllOverlap;
			}
			else if (selectedCollisionGroup == 8)
			{
				collisionGroup = CollisionGroup::Character;
			}
			else if (selectedCollisionGroup == 9)
			{
				collisionGroup = CollisionGroup::All;
			}
			else if (selectedCollisionGroup == 10)
			{
				collisionGroup = CollisionGroup::Custom0;
			}
			else if (selectedCollisionGroup == 11)
			{
				collisionGroup = CollisionGroup::Custom1;
			}
			else if (selectedCollisionGroup == 12)
			{
				collisionGroup = CollisionGroup::Custom2;
			}
			else if (selectedCollisionGroup == 13)
			{
				collisionGroup = CollisionGroup::Custom3;
			}
			else if (selectedCollisionGroup == 14)
			{
				collisionGroup = CollisionGroup::Custom4;
			}
			else if (selectedCollisionGroup == 15)
			{
				collisionGroup = CollisionGroup::Custom5;
			}
			else if (selectedCollisionGroup == 16)
			{
				collisionGroup = CollisionGroup::Custom6;
			}
			else if (selectedCollisionGroup == 17)
			{
				collisionGroup = CollisionGroup::Custom7;
			}
			else if (selectedCollisionGroup == 18)
			{
				collisionGroup = CollisionGroup::Custom8;
			}
			else if (selectedCollisionGroup == 19)
			{
				collisionGroup = CollisionGroup::Custom9;
			}
			rigidBody->SetCollisionGroup(collisionGroup);
		}
		ImGui::Columns(1, nullptr, false);

		static const char* collisionMaskNames[]{
			"",
			"Default",
			"BlockWorldDynamic",
			"BlockWorldStatic",
			"BlockCharacter",
			"BlockAll",
			"BlockAllExceptCharacter",
			"OverlapWorldDynamic",
			"OverlapWorldStatic",
			"OverlapCharacter",
			"OverlapAll",
			"OverlapAllExceptCharacter",
			"BlockAndOverlapAll",
			"Custom0",
			"Custom1",
			"Custom2",
			"Custom3",
			"Custom4",
			"Custom5",
			"Custom6",
			"Custom7",
			"Custom8",
			"Custom9"
		};
		int selectedCollisionMask = 1;

		CollisionMask collisionMask = rigidBody->GetCollisionMask();
		if (collisionMask == CollisionMask::Default) selectedCollisionMask = 1;
		else if (collisionMask == CollisionMask::BlockWorldDynamic) selectedCollisionMask = 2;
		else if (collisionMask == CollisionMask::BlockWorldStatic) selectedCollisionMask = 3;
		else if (collisionMask == CollisionMask::BlockCharacter) selectedCollisionMask = 4;
		else if (collisionMask == CollisionMask::BlockAll) selectedCollisionMask = 5;
		else if (collisionMask == CollisionMask::BlockAllExceptCharacter) selectedCollisionMask = 6;
		else if (collisionMask == CollisionMask::OverlapWorldDynamic) selectedCollisionMask = 7;
		else if (collisionMask == CollisionMask::OverlapWorldStatic) selectedCollisionMask = 8;
		else if (collisionMask == CollisionMask::OverlapCharacter) selectedCollisionMask = 9;
		else if (collisionMask == CollisionMask::OverlapAll) selectedCollisionMask = 10;
		else if (collisionMask == CollisionMask::OverlapAllExceptCharacter) selectedCollisionMask = 11;
		else if (collisionMask == CollisionMask::BlockAndOverlapAll) selectedCollisionMask = 12;
		else if (collisionMask == CollisionMask::Custom0) selectedCollisionMask = 13;
		else if (collisionMask == CollisionMask::Custom1) selectedCollisionMask = 14;
		else if (collisionMask == CollisionMask::Custom2) selectedCollisionMask = 15;
		else if (collisionMask == CollisionMask::Custom3) selectedCollisionMask = 16;
		else if (collisionMask == CollisionMask::Custom4) selectedCollisionMask = 17;
		else if (collisionMask == CollisionMask::Custom5) selectedCollisionMask = 18;
		else if (collisionMask == CollisionMask::Custom6) selectedCollisionMask = 19;
		else if (collisionMask == CollisionMask::Custom7) selectedCollisionMask = 20;
		else if (collisionMask == CollisionMask::Custom8) selectedCollisionMask = 21;
		else if (collisionMask == CollisionMask::Custom9) selectedCollisionMask = 22;

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, 125);
		ImGui::Text("CollisionMask: ");
		ImGui::NextColumn();
		ImGui::SetColumnWidth(1, 800);
		bool collisionMaskCheck = ImGui::Combo("##CollisionMask", &selectedCollisionMask, collisionMaskNames, IM_ARRAYSIZE(collisionMaskNames));
		if (collisionMaskCheck)
		{
			CollisionMask collisionMask = CollisionMask::Default;
			if (selectedCollisionMask == 2)
			{
				collisionMask = CollisionMask::BlockWorldDynamic;
			}
			else if (selectedCollisionMask == 3)
			{
				collisionMask = CollisionMask::BlockWorldStatic;
			}
			else if (selectedCollisionMask == 4)
			{
				collisionMask = CollisionMask::BlockCharacter;
			}
			else if (selectedCollisionMask == 5)
			{
				collisionMask = CollisionMask::BlockAll;
			}
			else if (selectedCollisionMask == 6)
			{
				collisionMask = CollisionMask::BlockAllExceptCharacter;
			}
			else if (selectedCollisionMask == 7)
			{
				collisionMask = CollisionMask::OverlapWorldDynamic;
			}
			else if (selectedCollisionMask == 8)
			{
				collisionMask = CollisionMask::OverlapWorldStatic;
			}
			else if (selectedCollisionMask == 9)
			{
				collisionMask = CollisionMask::OverlapCharacter;
			}
			else if (selectedCollisionMask == 10)
			{
				collisionMask = CollisionMask::OverlapAll;
			}
			else if (selectedCollisionMask == 11)
			{
				collisionMask = CollisionMask::OverlapAllExceptCharacter;
			}
			else if (selectedCollisionMask == 12)
			{
				collisionMask = CollisionMask::BlockAndOverlapAll;
			}
			else if (selectedCollisionMask == 13)
			{
				collisionMask = CollisionMask::Custom0;
			}
			else if (selectedCollisionMask == 14)
			{
				collisionMask = CollisionMask::Custom1;
			}
			else if (selectedCollisionMask == 15)
			{
				collisionMask = CollisionMask::Custom2;
			}
			else if (selectedCollisionMask == 16)
			{
				collisionMask = CollisionMask::Custom3;
			}
			else if (selectedCollisionMask == 17)
			{
				collisionMask = CollisionMask::Custom4;
			}
			else if (selectedCollisionMask == 18)
			{
				collisionMask = CollisionMask::Custom5;
			}
			else if (selectedCollisionMask == 19)
			{
				collisionMask = CollisionMask::Custom6;
			}
			else if (selectedCollisionMask == 20)
			{
				collisionMask = CollisionMask::Custom7;
			}
			else if (selectedCollisionMask == 21)
			{
				collisionMask = CollisionMask::Custom8;
			}
			else if (selectedCollisionMask == 22)
			{
				collisionMask = CollisionMask::Custom9;
			}
			rigidBody->SetCollisionMask(collisionMask);
		}
		ImGui::Columns(1, nullptr, false);
	}
	ImGui::Separator();

	const std::vector<Component*>& components = selectedObject->GetComponents();
	for (Component* component : components)
	{
		DrawDetailsWindow_Component(selectedObject, component);
		ImGui::Separator();
	}

	ImGui::PopItemWidth();
}

void EditorHUD::DrawGameOptionsBar()
{
	BeginWindow(gameOptionsWindowName_, ImGuiWindowFlags_NoTitleBar);

	static float timeScale = engine->GetTimeScale();

	if (ImGui::Button("Play", { 50.f, 50.f }))
	{
		engine->SetTimeScale(timeScale);
	}
	
	ImGui::SameLine();

	if(ImGui::Button("Pause", { 50.f, 50.f }))
	{
		timeScale = engine->GetTimeScale();
		engine->SetTimeScale(0.f);
	}

	EndWindow();
}

void EditorHUD::DrawDetailsWindow_AddComponentOptions(ObjectBase* object)
{
	static const char* objectBaseComponents[]{
		"",
		"StaticMeshComponent"
	};

	static const char* physicsObjectComponents[]{
		"",
		"StaticMeshComponent",
		"BoxCollisionComponent",
		"CapsuleCollisionComponent",
		"SphereCollisionComponent",
		"MovingTriangleMeshCollisionComponent",
		"NonMovingTriangleMeshCollisionComponent"
	};

	PhysicsObject* physicsObject = dynamic_cast<PhysicsObject*>(object);

	static int selectedItem = 0;
	if (physicsObject)
	{
		bool check = ImGui::Combo("##AddComponent", &selectedItem, physicsObjectComponents, IM_ARRAYSIZE(physicsObjectComponents));
		if (check)
		{
			std::string selectedComponentString = physicsObjectComponents[selectedItem];

			if (objectBaseReflections.find(selectedComponentString) != objectBaseReflections.end())
			{
				objectBaseReflections[selectedComponentString](physicsObject);
			}
			else if (physicsObjectReflections.find(selectedComponentString) != physicsObjectReflections.end())
			{
				physicsObjectReflections[selectedComponentString](physicsObject);
			}
			selectedItem = 0;
		}
	}
	else
	{
		bool check = ImGui::Combo("##AddComponent", &selectedItem, objectBaseComponents, IM_ARRAYSIZE(objectBaseComponents));
		if (check)
		{
			std::string selectedComponentString = objectBaseComponents[selectedItem];

			if (objectBaseReflections.find(selectedComponentString) != objectBaseReflections.end())
			{
				objectBaseReflections[selectedComponentString](object);
			}
		}
	}
}

void EditorHUD::DrawDetailsWindow_Component(ObjectBase* owner, Component* component)
{
	if (!component)
	{
		return;
	}

	StaticMeshComponent* staticMeshComponent{ nullptr };
	BoxCollisionComponent* boxCollisionComponent{ nullptr };
	SphereCollisionComponent* sphereCollisionComponent{ nullptr };
	CapsuleCollisionComponent* capsuleCollisionComponent{ nullptr };
	MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent{ nullptr };
	NonMovingTriangleMeshCollisionComponent* nonMvingTriangleMeshCollisionComponent{ nullptr };

	std::string componentTypeString;

	if (staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component))
	{
		componentTypeString = "StaticMeshComponent";
	}
	else if (boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component))
	{
		componentTypeString = "BoxCollisionComponent";
	}
	else if (sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component))
	{
		componentTypeString = "SphereCollisionComponent";
	}
	else if (capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
	{
		componentTypeString = "CapsuleCollisionComponent";
	}
	else if (movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
	{
		componentTypeString = "MovingTriangleMeshCollisionComponent";
	}
	else if (nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
	{
		componentTypeString = "NonMovingTriangleMeshCollisionComponent";
	}
	else
	{
		componentTypeString = "Component";
	}

	if (component->GetOwner()->GetRootComponent() == component)
	{
		componentTypeString += " (RootComponent)";
	}

	ImGui::Text(componentTypeString.c_str());

	ImGui::Separator();

	Vector3 componentRelativePosition = component->GetRelativePosition();
	Vector3 componentRelativeRotationEulerDegrees = component->GetRelativeRotation().ToEulerDegrees();
	Vector3 componentRelativeScaling = component->GetRelativeScaling();

	std::string specialPostfix = "##" + std::to_string(component->GetGUID());

	ImGui::Text("RelativePosition: ");
	ImGui::SameLine();
	DrawInputFloat(std::string("##RelativePosition") + specialPostfix, componentRelativePosition);
	component->SetRelativePosition(componentRelativePosition);

	ImGui::Text("RelativeRotation: ");
	ImGui::SameLine();
	DrawInputFloat(std::string("##RelativeRotation") + specialPostfix, componentRelativeRotationEulerDegrees);
	component->SetRelativeRotation(Quaternion::FromEulerDegrees(componentRelativeRotationEulerDegrees));

	ImGui::Text("RelativeScaling: ");
	ImGui::SameLine();
	DrawInputFloat(std::string("##RelativeScaling") + specialPostfix, componentRelativeScaling);
	component->SetRelativeScaling(componentRelativeScaling);

	if (staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component))
	{
		DrawDetailsWindow_StaticMeshComponent(staticMeshComponent);
	}
	else if (boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component))
	{
		DrawDetailsWindow_BoxCollisionComponent(boxCollisionComponent);
	}
	else if (sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component))
	{
		DrawDetailsWindow_SphereCollisionComponent(sphereCollisionComponent);
	}
	else if (capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
	{
		DrawDetailsWindow_CapsuleCollisionComponent(capsuleCollisionComponent);
	}
	else if (movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
	{
		DrawDetailsWindow_MovingTriangleMeshCollisionComponent(movingTriangleMeshCollisionComponent);
	}
	else if (nonMvingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
	{
		DrawDetailsWindow_NonMovingTriangleMeshCollisionComponent(nonMvingTriangleMeshCollisionComponent);
	}
}

void EditorHUD::DrawDetailsWindow_StaticMeshComponent(StaticMeshComponent* staticMeshComponent)
{
	std::string specialPostfix = "##" + std::to_string(staticMeshComponent->GetGUID());

	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	StaticMesh* staticMesh = staticMeshComponent->GetMeshInstance()->GetMesh();
	ImGui::Text(staticMesh ? staticMesh->GetPath().substr(ContentDir.size()).c_str() : "");

	std::string specialName = std::string("Select asset") + specialPostfix;

	ImGui::SameLine();
	if (ImGui::Button(specialName.c_str()))
	{
		windowOpenMap_[specialName.c_str()] = true;
	}

	if (windowOpenMap_[specialName.c_str()])
	{
		std::string selectedAssetPath;
		if (DrawAssetSelector(selectedAssetPath))
		{
			StaticMesh* newStaticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(selectedAssetPath);
			if (newStaticMesh)
			{
				staticMeshComponent->SetMesh(newStaticMesh);
				windowOpenMap_[specialName.c_str()] = false;
			}
		}
	}
}

void EditorHUD::DrawDetailsWindow_BoxCollisionComponent(BoxCollisionComponent* boxCollisionComponent)
{
}

void EditorHUD::DrawDetailsWindow_SphereCollisionComponent(SphereCollisionComponent* sphereCollisionComponent)
{
	ImGui::Text("Radius: ");
	ImGui::SameLine();
	float sphereCollisionComponentRadius = sphereCollisionComponent->GetRadius();
	DrawInputFloat((std::string("##SphereCollisionComponentRadius_") + std::to_string(sphereCollisionComponent->GetRadius())).c_str(), sphereCollisionComponentRadius);

	if (sphereCollisionComponentRadius != sphereCollisionComponent->GetRadius())
	{
		sphereCollisionComponent->SetRadius(sphereCollisionComponentRadius);
	}
}

void EditorHUD::DrawDetailsWindow_CapsuleCollisionComponent(CapsuleCollisionComponent* capsuleCollisionComponent)
{
	ImGui::Text("Radius: ");
	ImGui::SameLine();
	float capsuleCollisionComponentRadius = capsuleCollisionComponent->GetRadius();
	DrawInputFloat((std::string("##SphereCollisionComponentRadius_") + std::to_string(capsuleCollisionComponent->GetRadius())).c_str(), capsuleCollisionComponentRadius);

	if (capsuleCollisionComponentRadius != capsuleCollisionComponent->GetRadius())
	{
		capsuleCollisionComponent->SetRadius(capsuleCollisionComponentRadius);
	}

	ImGui::Text("Height: ");
	ImGui::SameLine();
	float capsuleCollisionComponentHeight = capsuleCollisionComponent->GetHeight();
	DrawInputFloat((std::string("##SphereCollisionComponentHeight_") + std::to_string(capsuleCollisionComponent->GetHeight())).c_str(), capsuleCollisionComponentHeight);

	if (capsuleCollisionComponentHeight != capsuleCollisionComponent->GetHeight())
	{
		capsuleCollisionComponent->SetHeight(capsuleCollisionComponentHeight);
	}
}

void EditorHUD::DrawDetailsWindow_MovingTriangleMeshCollisionComponent(MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const MeshUnit* mesh = movingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");
}

void EditorHUD::DrawDetailsWindow_NonMovingTriangleMeshCollisionComponent(NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent)
{
	std::string meshPath;
	ImGui::Text("Mesh: ");

	ImGui::SameLine();
	const MeshUnit* mesh = nonMovingTriangleMeshCollisionComponent->GetMesh();
	ImGui::Text(mesh ? mesh->GetPath().substr(ContentDir.size()).c_str() : "");

}

void EditorHUD::DrawDetailsWindow_DirectionalLight()
{
	DirectionalLight* light = static_cast<DirectionalLight*>(selectedObject_);

	static Vector3 lightPosition = light->GetPosition();
	static Vector3 lightDirection = light->GetDirection();
	static Vector3 lightColor = light->GetColor();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputFloat("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	DrawInputFloat("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	DrawInputFloat("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	DrawCheckbox("##IsCastingShadow", lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}

void EditorHUD::DrawDetailsWindow_PointLight()
{
	PointLight* light = static_cast<PointLight*>(selectedObject_);

	Vector3 lightPosition = light->GetPosition();
	Vector3 lightColor = light->GetColor();
	float lightIntensity = light->GetIntensity();
	float lightRadius = light->GetRadius();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();
	float lightShadowIntensity = light->GetShadowIntensity();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputFloat("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	DrawInputFloat("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Radius: ");
	ImGui::SameLine();
	DrawInputFloat("##Radius", lightRadius);
	light->SetRadius(lightRadius);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	DrawCheckbox("##IsCastingShadow", lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}

void EditorHUD::DrawDetailsWindow_SpotLight()
{
	SpotLight* light = static_cast<SpotLight*>(selectedObject_);

	Vector3 lightPosition = light->GetPosition();
	Vector3 lightDirection = light->GetDirection();
	Vector3 lightColor = light->GetColor();
	float lightFalloffAngle = light->GetFalloffAngle();
	float lightCoverageAngle = light->GetCoverageAngle();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(50.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputFloat("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	DrawInputFloat("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("FalloffAngle: ");
	ImGui::SameLine();
	DrawInputFloat("##FalloffAngle", lightFalloffAngle);
	light->SetFalloffAngle(lightFalloffAngle);

	ImGui::Text("CoverageAngle: ");
	ImGui::SameLine();
	DrawInputFloat("##CoverageAngle", lightCoverageAngle);
	light->SetCoverageAngle(lightCoverageAngle);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	DrawInputFloat("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	DrawCheckbox("##IsCastingShadow", lightIsShadowEnabled);
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	DrawInputFloat("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}

void EditorHUD::DrawInputText(const std::string& name, std::string& value)
{
	char* valueChar = const_cast<char*>(value.c_str());
	ImGui::InputText(name.c_str(), valueChar, 64);
	value = std::string(valueChar);
}

void EditorHUD::DrawInputFloat(const std::string& name, float& value)
{
	float newValue = value;
	ImGui::InputFloat((name).c_str(), &newValue);
	if (SMALLER_EPSILON < GoknarMath::Abs(value - newValue))
	{
		value = newValue;
	}
}

void EditorHUD::DrawInputFloat(const std::string& name, Vector3& vector)
{
	float newValueX = vector.x;
	ImGui::InputFloat((name + "X").c_str(), &newValueX);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.x - newValueX))
	{
		vector.x = newValueX;
	}

	ImGui::SameLine();

	float newValueY = vector.y;
	ImGui::InputFloat((name + "Y").c_str(), &newValueY);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.y - newValueY))
	{
		vector.y = newValueY;
	}

	ImGui::SameLine();

	float newValueZ = vector.z;
	ImGui::InputFloat((name + "Z").c_str(), &newValueZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.z - newValueZ))
	{
		vector.z = newValueZ;
	}
}

void EditorHUD::DrawInputFloat(const  std::string& name, Quaternion& quaternion)
{
	float newQuaternionX = quaternion.x;
	ImGui::InputFloat((name + "X").c_str(), &newQuaternionX);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.x - newQuaternionX))
	{
		quaternion.x = newQuaternionX;
	}

	ImGui::SameLine();

	float newQuaternionY = quaternion.y;
	ImGui::InputFloat((name + "Y").c_str(), &newQuaternionY);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.y - newQuaternionY))
	{
		quaternion.y = newQuaternionY;
	}

	ImGui::SameLine();

	float newQuaternionZ = quaternion.z;
	ImGui::InputFloat((name + "Z").c_str(), &newQuaternionZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.z - newQuaternionZ))
	{
		quaternion.z = newQuaternionZ;
	}

	ImGui::SameLine();

	float newQuaternionW = quaternion.w;
	ImGui::InputFloat((name + "W").c_str(), &newQuaternionW, 64);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.w - newQuaternionW))
	{
		quaternion.w = newQuaternionW;
	}
}

void EditorHUD::DrawCheckbox(const std::string& name, bool& value)
{
	ImGui::Checkbox(name.c_str(), &value);
}

bool EditorHUD::DrawAssetSelector(std::string& selectedAssetPath)
{
	ImVec2 assetPickerWindowSize(600.f, 600.f);
	ImGui::SetNextWindowPos(ImVec2((windowSize_.x - assetPickerWindowSize.x) * 0.5f, (windowSize_.y - assetPickerWindowSize.y) * 0.5f));
	ImGui::SetNextWindowSize(assetPickerWindowSize);
	BeginWindow("Asset Selector", ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize);

	std::string selectedAssetPathFromGrid;
	bool isAFileSelected = false;
	DrawFileGrid(rootFolder, selectedAssetPathFromGrid, isAFileSelected);

	if (isAFileSelected)
	{
		selectedAssetPath = selectedAssetPathFromGrid;
		windowOpenMap_["Asset Selector"] = false;
	}

	EndWindow();

	return isAFileSelected;
}

void EditorHUD::BeginWindow(const std::string& name, ImGuiWindowFlags flags)
{
	if (windowOpenMap_.find(name) == windowOpenMap_.end())
	{
		windowOpenMap_[name] = true;
	}

	ImGui::Begin(name.c_str(), &windowOpenMap_[name], flags);
}

void EditorHUD::BeginTransparentWindow(const std::string& name, ImGuiWindowFlags additionalFlags)
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;
	windowFlags |= ImGuiWindowFlags_NoTitleBar;
	windowFlags |= additionalFlags;

	BeginWindow(name, windowFlags);
}

void EditorHUD::EndWindow()
{
	ImGui::End();
}

bool EditorHUD::BeginDialogWindow_OneTextBoxOneButton(const std::string& windowTitle, const std::string& text, const std::string& currentValue, const std::string& buttonText, ImGuiWindowFlags flags)
{
	ImVec2 windowSize(400.f, 400.f);
	ImGui::SetNextWindowPos(ImVec2((windowSize_.x - windowSize.x) * 0.5f, (windowSize_.y - windowSize.y) * 0.5f));
	ImGui::SetNextWindowSize(windowSize);

	flags |= ImGuiWindowFlags_NoDocking;

	BeginWindow(windowTitle, flags);

	ImGui::Text(text.c_str());
	ImGui::SameLine();

	char* input = const_cast<char*>(currentValue.c_str());
	ImGui::InputText((std::string("##") + windowTitle + "_TextBox").c_str(), input, 1024);

	std::string inputString = input;
	bool buttonClicked = ImGui::Button(buttonText.c_str());
	buttonClicked = buttonClicked && !inputString.empty();

	if (buttonClicked)
	{
		sceneSavePath_ = inputString;
	}

	ImGui::End();

	return buttonClicked;
}

void EditorHUD::FocusToPosition(const Vector3& position)
{
	FreeCameraObject* freeCameraObject = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject();
	freeCameraObject->SetWorldPosition(position - 20.f * freeCameraObject->GetForwardVector());
}

void EditorHUD::OpenSaveSceneDialog()
{
	ImVec2 windowSize(400.f, 100.f);
	ImGui::SetNextWindowPos(ImVec2((windowSize_.x - windowSize.x) * 0.5f, (windowSize_.y - windowSize.y) * 0.5f));
	ImGui::SetNextWindowSize(windowSize);

	if (BeginDialogWindow_OneTextBoxOneButton(saveSceneDialogWindowName_, "Path: ", sceneSavePath_, "Save", ImGuiWindowFlags_NoResize))
	{
		SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + sceneSavePath_);

		windowOpenMap_[saveSceneDialogWindowName_] = false;
	}
}

DirectionalLight* EditorHUD::CreateDirectionalLight()
{
	DirectionalLight* newDirectionalLight = new DirectionalLight();
	selectedObjectType_ = Editor_ObjectType::DirectionalLight;
	selectedObject_ = newDirectionalLight;

	return newDirectionalLight;
}

PointLight* EditorHUD::CreatePointLight()
{
	PointLight* newPointLight = new PointLight();
	selectedObjectType_ = Editor_ObjectType::PointLight;
	selectedObject_ = newPointLight;
	return newPointLight;
}

SpotLight* EditorHUD::CreateSpotLight()
{
	SpotLight* newSpotLight = new SpotLight();
	selectedObjectType_ = Editor_ObjectType::SpotLight;
	selectedObject_ = newSpotLight;
	return newSpotLight;
}

ObjectBase* EditorHUD::CreateObject(const std::string& typeName)
{
	ObjectBase* newObjectBase = DynamicObjectFactory::GetInstance()->Create(typeName);

	if (!newObjectBase)
	{
		return nullptr;
	}

	newObjectBase->SetName(typeName);
	selectedObjectType_ = Editor_ObjectType::Object;
	selectedObject_ = newObjectBase;

	return newObjectBase;
}

void EditorHUD::DrawObjectNameToCreateWindow()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetWindow(), x, y);
	ImGui::SetNextWindowPos({(float)x, (float)y});
	BeginTransparentWindow("ObjectToCreateNameWindow", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
	ImGui::Text(objectToCreateName_.c_str());
	EndWindow();
}
