#include "EditorHUD.h"

#include <string>
#include <unordered_map>

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"

#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"

#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"

#include "Goknar/Debug/DebugDrawer.h"

#include "Goknar/Factories/DynamicObjectFactory.h"

#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Goknar/Model/MeshUnit.h"

#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"

#include "Goknar/Renderer/Texture.h"
#include "Goknar/Renderer/RenderTarget.h"

#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "Goknar/Helpers/SceneParser.h"

#include "Game.h"
#include "Controllers/FreeCameraController.h"
#include "Objects/FreeCameraObject.h"
#include "Thirdparty/ImGuiOpenGL.h"
#include "EditorWidgets.h"

#include "EditorContext.h"
#include "Panels/DebugPanel.h"
#include "Panels/DetailsPanel.h"
#include "Panels/FileBrowserPanel.h"
#include "Panels/GeometryBuffersPanel.h"
#include "Panels/MenuBar.h"
#include "Panels/SaveScenePanel.h"
#include "Panels/ScenePanel.h"
#include "Panels/ViewportPanel.h"

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
	engine->SetHUD(this);

	AddPanel<MenuBar>();
	AddPanel<DetailsPanel>();
	AddPanel<FileBrowserPanel>();
	AddPanel<SaveScenePanel>();
	AddPanel<ScenePanel>();
	AddPanel<ViewportPanel>();
	AddPanel<DebugPanel>();

	if(engine->GetRenderer()->GetMainRenderType() == RenderPassType::Deferred)
	{
		AddPanel<GeometryBuffersPanel>();
	}

	onKeyboardEventDelegate_ = Delegate<void(int, int, int, int)>::Create<EditorHUD, &EditorHUD::OnKeyboardEvent>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::Create<EditorHUD, &EditorHUD::OnCursorMove>(this);
	onScrollDelegate_ = Delegate<void(double, double)>::Create<EditorHUD, &EditorHUD::OnScroll>(this);
	onLeftClickPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnLeftClickPressed>(this);
	onLeftClickReleasedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnLeftClickReleased>(this);
	onCharPressedDelegate_ = Delegate<void(unsigned int)>::Create<EditorHUD, &EditorHUD::OnCharPressed>(this);
	onWindowSizeChangedDelegate_ = Delegate<void(int, int)>::Create<EditorHUD, &EditorHUD::OnWindowSizeChanged>(this);
	onDeleteInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnDeleteInputPressed>(this);
	onFocusInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnFocusInputPressed>(this);
	onCancelInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnCancelInputPressed>(this);

	uiImage_ = engine->GetResourceManager()->GetContent<Image>("Textures/UITexture.png");


	engine->GetRenderer()->SetDrawOnWindow(false);
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

	delete EditorContext::Get();
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

	std::vector<std::unique_ptr<IEditorPanel>>::const_iterator panelIterator = panels_.cbegin();
	while (panelIterator != panels_.cend())
	{
		panelIterator->get()->Init();
		++panelIterator;
	}
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

void EditorHUD::SetupDocking()
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
	ImGui::End();
}

void EditorHUD::OnKeyboardEvent(int key, int scanCode, int action, int mod)
{
	ImGuiIO& io = ImGui::GetIO();
	bool is_down = (action == GLFW_PRESS || action == GLFW_REPEAT);
	ImGuiKey imgui_key = ImGui_ImplGlfw_KeyToImGuiKey(key, scanCode);
	io.AddKeyEvent(imgui_key, is_down);
}

void EditorHUD::OnCursorMove(double xPosition, double yPosition)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos.x = (float)xPosition;
	io.MousePos.y = (float)yPosition;
}

void EditorHUD::OnScroll(double xOffset, double yOffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xOffset;
	io.MouseWheel += (float)yOffset;
}

void EditorHUD::OnLeftClickPressed()
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[GLFW_MOUSE_BUTTON_1] = true;

	if (EditorContext::Get()->objectToCreateType != EditorSelectionType::None)
	{
		OnPlaceObject();
	}
}

void EditorHUD::OnLeftClickReleased()
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[GLFW_MOUSE_BUTTON_1] = false;
}

void EditorHUD::OnCharPressed(unsigned int codePoint)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(codePoint);
}

void EditorHUD::OnWindowSizeChanged(int width, int height)
{
	EditorContext::Get()->windowSize = Vector2i(width, height);
	Vector2 buttonSizeVector = EditorContext::Get()->windowSize * 0.05f;
	EditorContext::Get()->buttonSize = Vector2i(buttonSizeVector.x, buttonSizeVector.y);
}

void EditorHUD::OnPlaceObject()
{
	Vector3 raycastPosition = RaycastWorld();

	switch (EditorContext::Get()->objectToCreateType)
	{
	case EditorSelectionType::Object:
	{
		ObjectBase* object = CreateObject(EditorContext::Get()->objectToCreateName);
		if (object)
		{
			object->SetWorldPosition(raycastPosition);
		}
		break;
	}
	case EditorSelectionType::DirectionalLight:
	{
		CreateDirectionalLight();
		break;
	}
	case EditorSelectionType::PointLight:
	{
		PointLight* pointLight = CreatePointLight();
		pointLight->SetPosition(raycastPosition);
		break;
	}
	case EditorSelectionType::SpotLight:
	{
		CreateSpotLight();
		SpotLight* spotLight = CreateSpotLight();
		spotLight->SetPosition(raycastPosition);
		break;
	}
	case EditorSelectionType::None:
	default:
		break;
	}

	EditorContext::Get()->objectToCreateType = EditorSelectionType::None;
	EditorContext::Get()->objectToCreateName = "";
}

Vector3 EditorHUD::RaycastWorld()
{
	Camera* activeCamera = EditorContext::Get()->viewportRenderTarget->GetCamera();

	InputManager* inputManager = engine->GetInputManager();

	double x, y;
	inputManager->GetCursorPosition(x, y);

	Vector3 cameraWorldPosition = activeCamera->GetPosition();
	Vector3 cameraForwardVector = activeCamera->GetWorldDirectionAtPixel(Vector2i{ (int)(x - viewportPosition_.x), (int)(y - viewportPosition_.y) });

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
	switch (EditorContext::Get()->selectedObjectType)
	{
	case EditorSelectionType::Object:
		((ObjectBase*)EditorContext::Get()->selectedObject)->Destroy();
		break;
	case EditorSelectionType::DirectionalLight:
		delete ((DirectionalLight*)EditorContext::Get()->selectedObject);
		break;
	case EditorSelectionType::PointLight:
		delete ((PointLight*)EditorContext::Get()->selectedObject);
		break;
	case EditorSelectionType::SpotLight:
		delete ((SpotLight*)EditorContext::Get()->selectedObject);
		break;
	case EditorSelectionType::None:
	default:
		return;
	}

	EditorContext::Get()->selectedObjectType = EditorSelectionType::None;
	EditorContext::Get()->selectedObject = nullptr;
}

void EditorHUD::OnFocusInputPressed()
{
	Vector3 position = Vector3::ZeroVector;
	switch (EditorContext::Get()->selectedObjectType)
	{
	case EditorSelectionType::Object:
		position = ((ObjectBase*)EditorContext::Get()->selectedObject)->GetWorldPosition();
		break;
	case EditorSelectionType::DirectionalLight:
	case EditorSelectionType::PointLight:
	case EditorSelectionType::SpotLight:
		position = ((Light*)EditorContext::Get()->selectedObject)->GetPosition();
		break;
	case EditorSelectionType::None:
	default:
		return;
	}

	FocusToPosition(position);
}

void EditorHUD::OnCancelInputPressed()
{
	EditorContext::Get()->ClearCreateData();
}

void EditorHUD::UpdateHUD()
{
	HUD::UpdateHUD();

	WindowManager* windowManager = engine->GetWindowManager();
	EditorContext::Get()->windowSize = windowManager->GetWindowSize();

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
	if (!engine->GetRenderer()->GetDrawOnWindow())
	{
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	SetupDocking();

	//DrawMenuBar();

	std::vector<std::unique_ptr<IEditorPanel>>::const_iterator panelIterator = panels_.cbegin();
	while (panelIterator != panels_.cend())
	{
		IEditorPanel* panel = panelIterator->get();

		if (panel->GetIsOpen())
		{
			panel->Draw();
		}

		++panelIterator;
	}

	return;

	DrawDrawInfo();

	if (EditorContext::Get()->objectToCreateType != EditorSelectionType::None)
	{
		DrawObjectNameToCreateWindow();
	}

	EditorContext::Get()->viewportRenderTarget->SetIsActive(true);// windowOpenMap_[viewportWindowName_] || shouldDrawGeometryBuffersWindow);

	EditorContext::Get()->viewportCamera->GetFreeCameraController()->SetIsActive(shouldFreeCameraControllerBeEnabled_);
}

void EditorHUD::DrawDrawInfo()
{
}

void EditorHUD::DrawObjectsWindow()
{
	BeginWindow("Objects", dockableWindowFlags_);
	if (ImGui::TreeNode("Lights"))
	{
		if (ImGui::Button("Directional Light"))
		{
			EditorContext::Get()->objectToCreateType = EditorSelectionType::DirectionalLight;
			EditorContext::Get()->objectToCreateName = "Directional Light";
		}
		else if (ImGui::Button("Point Light"))
		{
			EditorContext::Get()->objectToCreateType = EditorSelectionType::PointLight;
			EditorContext::Get()->objectToCreateName = "Point Light";
		}
		else if (ImGui::Button("Spot Light"))
		{
			EditorContext::Get()->objectToCreateType = EditorSelectionType::SpotLight;
			EditorContext::Get()->objectToCreateName = "Spot Light";
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
				EditorContext::Get()->objectToCreateType = EditorSelectionType::Object;
				EditorContext::Get()->objectToCreateName = name;
			}
		}

		ImGui::TreePop();
	}
	ImGui::Separator();
	EndWindow();
}

void EditorHUD::DrawGameOptionsBar()
{
	BeginWindow("GameOptions", ImGuiWindowFlags_NoTitleBar);

	static float timeScale = engine->GetTimeScale();

	if (ImGui::Button("Play", { 50.f, 50.f }))
	{
		engine->SetTimeScale(timeScale);
	}

	ImGui::SameLine();

	if (ImGui::Button("Pause", { 50.f, 50.f }))
	{
		timeScale = engine->GetTimeScale();
		engine->SetTimeScale(0.f);
	}

	EndWindow();
}

void EditorHUD::DrawCheckbox(const std::string& name, bool& value)
{
	ImGui::Checkbox(name.c_str(), &value);
}

//bool EditorHUD::DrawAssetSelector(std::string& selectedAssetPath)
//{
//	ImVec2 assetPickerWindowSize(600.f, 600.f);
//	ImGui::SetNextWindowPos(ImVec2((windowSize_.x - assetPickerWindowSize.x) * 0.5f, (windowSize_.y - assetPickerWindowSize.y) * 0.5f));
//	ImGui::SetNextWindowSize(assetPickerWindowSize);
//	BeginWindow("Asset Selector", ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize);
//
//	std::string selectedAssetPathFromGrid;
//	bool isAFileSelected = false;
//	DrawFileGrid(rootFolder, selectedAssetPathFromGrid, isAFileSelected);
//
//	if (isAFileSelected)
//	{
//		selectedAssetPath = selectedAssetPathFromGrid;
//		windowOpenMap_["Asset Selector"] = false;
//	}
//
//	EndWindow();
//
//	return isAFileSelected;
//}

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

void EditorHUD::FocusToPosition(const Vector3& position)
{
	EditorContext::Get()->viewportCamera->SetWorldPosition(position - 20.f * EditorContext::Get()->viewportCamera->GetForwardVector());
}

DirectionalLight* EditorHUD::CreateDirectionalLight()
{
	DirectionalLight* newDirectionalLight = new DirectionalLight();
	EditorContext::Get()->selectedObjectType = EditorSelectionType::DirectionalLight;
	EditorContext::Get()->selectedObject = newDirectionalLight;

	return newDirectionalLight;
}

PointLight* EditorHUD::CreatePointLight()
{
	PointLight* newPointLight = new PointLight();
	EditorContext::Get()->selectedObjectType = EditorSelectionType::PointLight;
	EditorContext::Get()->selectedObject = newPointLight;
	return newPointLight;
}

SpotLight* EditorHUD::CreateSpotLight()
{
	SpotLight* newSpotLight = new SpotLight();
	EditorContext::Get()->selectedObjectType = EditorSelectionType::SpotLight;
	EditorContext::Get()->selectedObject = newSpotLight;
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
	EditorContext::Get()->selectedObjectType = EditorSelectionType::Object;
	EditorContext::Get()->selectedObject = newObjectBase;

	return newObjectBase;
}

void EditorHUD::DrawObjectNameToCreateWindow()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetMainWindow(), x, y);
	ImGui::SetNextWindowPos({ (float)x, (float)y });
	BeginTransparentWindow("ObjectToCreateNameWindow", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
	ImGui::Text(EditorContext::Get()->objectToCreateName.c_str());
	EndWindow();
}