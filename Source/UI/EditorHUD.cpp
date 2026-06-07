#include "EditorHUD.h"

#include <string>
#include <unordered_map>

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Application.h"
#include "Goknar/Scene.h"
#include "Goknar/Graphics/IGraphicsAPI.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Helpers/SceneParser.h"

#include "Goknar/Contents/Image.h"

#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"

#include "Goknar/Factories/DynamicObjectFactory.h"

#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"

#include "Goknar/Renderer/Texture.h"
#include "Goknar/Renderer/RenderTarget.h"

#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "Controllers/EditorFreeCameraController.h"
#include "Objects/EditorFreeCameraObject.h"
#include "Thirdparty/EditorImGuiOpenGL.h"

#include "EditorContext.h"
#include "EditorAssetPathUtils.h"
#include "EditorUtils.h"

#include "Panels/AnimationGraphPanel.h"
#include "Panels/AssetSelectorPanel.h"
#include "Panels/DebugPanel.h"
#include "Panels/DetailsPanel.h"
#include "Panels/FileBrowserPanel.h"
#include "Panels/GeometryBuffersPanel.h"
#include "Panels/ImageViewerPanel.h"
#include "Panels/MenuBarPanel.h"
#include "Panels/MeshViewerPanel.h"
#include "Panels/NavigationPanel.h"
#include "Panels/ObjectCreationPanel.h"
#include "Panels/ObjectNameToCreatePanel.h"
#include "Panels/ParticleSystemPanel.h"
#include "Panels/ProjectSettingsPanel.h"
#include "Panels/SaveScenePanel.h"
#include "Panels/ScenePanel.h"
#include "Panels/ShaderEditor/ShaderEditorPanel.h"
#include "Panels/SkeletalMeshViewerPanel.h"
#include "Panels/SystemFileBrowserPanel.h"
#include "Panels/ToolBarPanel.h"
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

namespace
{
	struct SceneContentCounts
	{
		size_t directionalLightCount{ 0 };
		size_t pointLightCount{ 0 };
		size_t spotLightCount{ 0 };
		size_t textureCount{ 0 };
	};

	SceneContentCounts CaptureSceneContentCounts(Scene* scene)
	{
		SceneContentCounts counts;
		if (!scene)
		{
			return counts;
		}

		counts.directionalLightCount = scene->GetDirectionalLights().size();
		counts.pointLightCount = scene->GetPointLights().size();
		counts.spotLightCount = scene->GetSpotLights().size();
		counts.textureCount = scene->GetTextures().size();
		return counts;
	}

	template <typename T>
	void InitializeRange(const std::vector<T*>& items, size_t startIndex)
	{
		for (size_t itemIndex = startIndex; itemIndex < items.size(); ++itemIndex)
		{
			T* item = items[itemIndex];
			if (!item)
			{
				continue;
			}

			item->PreInit();
			item->Init();
			item->PostInit();
		}
	}

	void InitializeSceneContentAddedAfter(Scene* scene, const SceneContentCounts& counts)
	{
		if (!scene)
		{
			return;
		}

		InitializeRange(scene->GetDirectionalLights(), counts.directionalLightCount);
		InitializeRange(scene->GetPointLights(), counts.pointLightCount);
		InitializeRange(scene->GetSpotLights(), counts.spotLightCount);
		InitializeRange(scene->GetTextures(), counts.textureCount);
	}

	bool IsSceneAssetPath(EditorContext* context, const std::string& scenePath)
	{
		if (!context)
		{
			return false;
		}

		const std::string contentRelativePath = EditorAssetPathUtils::ToContentRelativePath(scenePath);
		if (contentRelativePath.empty())
		{
			return false;
		}

		if (context->GetAssetType("Content/" + contentRelativePath) == EditorAssetType::Scene)
		{
			return true;
		}

		return context->GetAssetType(scenePath) == EditorAssetType::Scene;
	}
}

EditorHUD::EditorHUD() : HUD()
{
	SetName("__Editor__HUD");

	engine->SetHUD(this);

	AddPanel<AnimationGraphPanel>();
	AddPanel<AssetSelectorPanel>();
	AddPanel<DebugPanel>();
	AddPanel<DetailsPanel>();
	AddPanel<FileBrowserPanel>();
	AddPanel<ImageViewerPanel>();
	AddPanel<MenuBarPanel>();
	AddPanel<MeshViewerPanel>();
	AddPanel<NavigationPanel>();
	AddPanel<ObjectCreationPanel>();
	AddPanel<ObjectNameToCreatePanel>();
	AddPanel<ParticleSystemPanel>();
	AddPanel<ProjectSettingsPanel>();
	AddPanel<SaveScenePanel>();
	AddPanel<ScenePanel>();
	AddPanel<ShaderEditorPanel>();
	AddPanel<SkeletalMeshViewerPanel>();
	AddPanel<SystemFileBrowserPanel>();
	AddPanel<ToolBarPanel>();

	if((unsigned char)engine->GetRenderer()->GetMainRenderType() & (unsigned char)RenderPassType::Deferred)
	{
		AddPanel<GeometryBuffersPanel>();
	}

	AddPanel<ViewportPanel>();

	onKeyboardEventDelegate_ = Delegate<void(int, int, int, int)>::Create<EditorHUD, &EditorHUD::OnKeyboardEvent>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::Create<EditorHUD, &EditorHUD::OnCursorMove>(this);
	onScrollDelegate_ = Delegate<void(double, double)>::Create<EditorHUD, &EditorHUD::OnScroll>(this);
	onLeftClickPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnLeftClickPressed>(this);
	onLeftClickReleasedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnLeftClickReleased>(this);
	onRightClickPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnRightClickPressed>(this);
	onRightClickReleasedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnRightClickReleased>(this);
	onMiddleClickPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnMiddleClickPressed>(this);
	onMiddleClickReleasedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnMiddleClickReleased>(this);
	onCharPressedDelegate_ = Delegate<void(unsigned int)>::Create<EditorHUD, &EditorHUD::OnCharPressed>(this);
	onWindowSizeChangedDelegate_ = Delegate<void(int, int)>::Create<EditorHUD, &EditorHUD::OnWindowSizeChanged>(this);
	onDeleteInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnDeleteInputPressed>(this);
	onFocusInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnFocusInputPressed>(this);
	onCancelInputPressedDelegate_ = Delegate<void()>::Create<EditorHUD, &EditorHUD::OnCancelInputPressed>(this);

	uiImage_ = EditorUtils::GetEditorContent<Image>("Textures/UI/T_UI.png");
	uiImage_->SetCanUseTextureAtlas(false);

	engine->GetRenderer()->SetDrawOnWindow(true);

	context_ = EditorContext::Get();
}

EditorHUD::~EditorHUD()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->RemoveKeyboardListener(onKeyboardEventDelegate_);

	inputManager->RemoveCursorDelegate(onCursorMoveDelegate_);
	inputManager->RemoveScrollDelegate(onScrollDelegate_);

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_PRESS, onLeftClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_RELEASE, onLeftClickReleasedDelegate_);

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_2, INPUT_ACTION::G_PRESS, onRightClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_2, INPUT_ACTION::G_RELEASE, onRightClickReleasedDelegate_);

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_3, INPUT_ACTION::G_PRESS, onMiddleClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_3, INPUT_ACTION::G_RELEASE, onMiddleClickReleasedDelegate_);

	inputManager->RemoveCharDelegate(onCharPressedDelegate_);

	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::DLT, INPUT_ACTION::G_PRESS, onDeleteInputPressedDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::F, INPUT_ACTION::G_PRESS, onFocusInputPressedDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::ESCAPE, INPUT_ACTION::G_PRESS, onCancelInputPressedDelegate_);

	engine->GetWindowManager()->RemoveWindowSizeCallback(onWindowSizeChangedDelegate_);

	EditorImGui_DestroyFontsTexture();
	EditorImGui_DestroyDeviceObjects();
	EditorImGui_Shutdown();

	EditorContext::Destroy();
	context_ = nullptr;
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

	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_2, INPUT_ACTION::G_PRESS, onRightClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_2, INPUT_ACTION::G_RELEASE, onRightClickReleasedDelegate_);

	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_3, INPUT_ACTION::G_PRESS, onMiddleClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_3, INPUT_ACTION::G_RELEASE, onMiddleClickReleasedDelegate_);

	inputManager->AddCharDelegate(onCharPressedDelegate_);

	inputManager->AddKeyboardInputDelegate(KEY_MAP::DLT, INPUT_ACTION::G_PRESS, onDeleteInputPressedDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::F, INPUT_ACTION::G_PRESS, onFocusInputPressedDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::ESCAPE, INPUT_ACTION::G_PRESS, onCancelInputPressedDelegate_);
}

void EditorHUD::Init()
{
	HUD::Init();

	context_->Init();

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
	windowManager->AddWindowSizeCallback(onWindowSizeChangedDelegate_);
	const Vector2i windowSize = windowManager->GetWindowSize();
	OnWindowSizeChanged(windowSize.x, windowSize.y);

	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.KeyRepeatRate = 0.25f;

	EditorImGui_Init("#version 410 core");

	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);
	colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.f);

	double cursorX = 0.0;
	double cursorY = 0.0;
	engine->GetInputManager()->GetCursorPosition(cursorX, cursorY);
	OnCursorMove(cursorX, cursorY);

	// For counting draw calls
	engine->GetCameraManager()->SetActiveCamera(EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera());
}

void EditorHUD::UpdateHUD()
{
	HUD::UpdateHUD();

	if (!engine->GetRenderer()->GetDrawOnWindow())
	{
		engine->GetGraphicsAPI()->ClearColor(0.f, 0.f, 0.f, 1.f);
		engine->GetGraphicsAPI()->Clear(GraphicsClearBuffer::Color | GraphicsClearBuffer::Depth);
	}

	WindowManager* windowManager = engine->GetWindowManager();
	context_->windowSize = windowManager->GetWindowSize();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = EditorUtils::ToImVec2(context_->windowSize);
	const Vector2i framebufferSize = windowManager->GetFramebufferSize();
	if (0 < context_->windowSize.x && 0 < context_->windowSize.y)
	{
		io.DisplayFramebufferScale = ImVec2(
			(float)framebufferSize.x / (float)context_->windowSize.x,
			(float)framebufferSize.y / (float)context_->windowSize.y);
	}
	else
	{
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}

	EditorImGui_NewFrame();
	ImGui::NewFrame();

	ImGui::StyleColorsDark();

	DrawBackgroundWindow();
	DrawEditorHUD();

	ImGui::Render();
	EditorImGui_RenderDrawData(ImGui::GetDrawData());

	doubleClickController_.ClearFrameState();
}

void EditorHUD::DrawBackgroundWindow()
{
	static bool p_open = true;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float menuBarHeight = ImGui::GetFrameHeight();
	const float topBarHeight = menuBarHeight + ToolBarPanel::GetToolbarHeight();

	ImVec2 dockspacePos = viewport->Pos;
	dockspacePos.y += topBarHeight;

	ImVec2 dockspaceSize = viewport->Size;
	dockspaceSize.y -= topBarHeight;

	ImGui::SetNextWindowPos(dockspacePos);
	ImGui::SetNextWindowSize(dockspaceSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	window_flags |= ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("Goknar Engine Editor", &p_open, window_flags);

	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("Editor");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

	ImGui::End();
}

void EditorHUD::DrawEditorHUD()
{
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

	context_->viewportRenderTarget->SetIsActive(true);
}

void EditorHUD::OnKeyboardEvent(int key, int scanCode, int action, int mod)
{
	ImGuiIO& io = ImGui::GetIO();
	bool is_down = (action == GLFW_PRESS || action == GLFW_REPEAT);
	ImGuiKey imgui_key = EditorImGui_ImplGlfw_KeyToImGuiKey(key, scanCode);
	io.AddKeyEvent(imgui_key, is_down);

	io.AddKeyEvent(ImGuiMod_Ctrl, (mod & GLFW_MOD_CONTROL) != 0);
	io.AddKeyEvent(ImGuiMod_Shift, (mod & GLFW_MOD_SHIFT) != 0);
	io.AddKeyEvent(ImGuiMod_Alt, (mod & GLFW_MOD_ALT) != 0);
	io.AddKeyEvent(ImGuiMod_Super, (mod & GLFW_MOD_SUPER) != 0);

	if (action == GLFW_PRESS && key == (int)KEY_MAP::D && (mod & GLFW_MOD_CONTROL) != 0)
	{
		OnCloneInputPressed();
	}
}

void EditorHUD::OnCursorMove(double xPosition, double yPosition)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent((float)xPosition, (float)yPosition);
	doubleClickController_.OnCursorMove(ImVec2((float)xPosition, (float)yPosition));
}

void EditorHUD::OnScroll(double xOffset, double yOffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseWheelEvent((float)xOffset, (float)yOffset);
}

void EditorHUD::OnLeftClickPressed()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
	doubleClickController_.OnMouseButtonPressed(ImGuiMouseButton_Left, GetCursorPositionForUi());

	if (NavigationPanel* navigationPanel = GetPanel<NavigationPanel>())
	{
		if (navigationPanel->HandleViewportLeftClick())
		{
			return;
		}
	}

	if (context_->objectToCreateType != EditorSelectionType::None)
	{
		OnPlaceObject();
		return;
	}

	if (ViewportPanel* viewportPanel = GetPanel<ViewportPanel>())
	{
		if (viewportPanel->HandleViewportLeftClick())
		{
			return;
		}
	}
}

void EditorHUD::OnLeftClickReleased()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
}

void EditorHUD::OnRightClickPressed()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
	doubleClickController_.OnMouseButtonPressed(ImGuiMouseButton_Right, GetCursorPositionForUi());

	if (io.KeyCtrl)
	{
		if (ViewportPanel* viewportPanel = GetPanel<ViewportPanel>())
		{
			if (viewportPanel->HandleViewportRightClick())
			{
				return;
			}
		}
	}
}

void EditorHUD::OnRightClickReleased()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
}

void EditorHUD::OnMiddleClickPressed()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
	doubleClickController_.OnMouseButtonPressed(ImGuiMouseButton_Middle, GetCursorPositionForUi());
}

void EditorHUD::OnMiddleClickReleased()
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
}

void EditorHUD::OnCharPressed(unsigned int codePoint)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(codePoint);
}

void EditorHUD::OnWindowSizeChanged(int width, int height)
{
	context_->windowSize = Vector2i(width, height);
	Vector2 buttonSizeVector = context_->windowSize * 0.05f;
	context_->buttonSize = Vector2i((int)buttonSizeVector.x, (int)buttonSizeVector.y);

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = EditorUtils::ToImVec2(context_->windowSize);
	const Vector2i framebufferSize = engine->GetWindowManager()->GetFramebufferSize();
	if (0 < width && 0 < height)
	{
		io.DisplayFramebufferScale = ImVec2((float)framebufferSize.x / (float)width, (float)framebufferSize.y / (float)height);
	}
	else
	{
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}
}

bool EditorHUD::WasMouseDoubleClicked(ImGuiMouseButton button) const
{
	return doubleClickController_.WasMouseDoubleClicked(button);
}

bool EditorHUD::WasLastItemDoubleClicked(ImGuiMouseButton button) const
{
	return WasMouseDoubleClicked(button) && ImGui::IsItemHovered();
}

ImVec2 EditorHUD::GetCursorPositionForUi() const
{
	double xPosition = 0.0;
	double yPosition = 0.0;

	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	return ImVec2((float)xPosition, (float)yPosition);
}

void EditorHUD::OnPlaceObject()
{
	Vector3 raycastPosition = RaycastWorld();
	bool didCreateSceneContent = false;

	switch (context_->objectToCreateType)
	{
	case EditorSelectionType::Object:
	{
		ObjectBase* object = CreateObject(context_->objectToCreateName);
		if (object)
		{
			object->SetWorldPosition(raycastPosition);
			didCreateSceneContent = true;
		}
		break;
	}
	case EditorSelectionType::DirectionalLight:
	{
		CreateDirectionalLight();
		didCreateSceneContent = true;
		break;
	}
	case EditorSelectionType::PointLight:
	{
		PointLight* pointLight = CreatePointLight();
		pointLight->SetPosition(raycastPosition);
		didCreateSceneContent = true;
		break;
	}
	case EditorSelectionType::SpotLight:
	{
		SpotLight* spotLight = CreateSpotLight();
		spotLight->SetPosition(raycastPosition);
		didCreateSceneContent = true;
		break;
	}
	case EditorSelectionType::Scene:
	{
		InsertSceneReference(context_->objectToCreateName, raycastPosition);
		break;
	}
	case EditorSelectionType::None:
	default:
		break;
	}

	context_->isPlacingObject = false;
	context_->objectToCreateType = EditorSelectionType::None;
	context_->objectToCreateName = "";

	HidePanel<ObjectNameToCreatePanel>();

	if (didCreateSceneContent)
	{
		context_->MarkSceneDirty("Scene object created");
	}
}

Vector3 EditorHUD::RaycastWorld()
{
	Camera* activeCamera = context_->viewportRenderTarget->GetCamera();

	InputManager* inputManager = engine->GetInputManager();

	double x, y;
	inputManager->GetCursorPosition(x, y);

	const Vector2i& viewportPosition = GetPanel<ViewportPanel>()->GetPosition();

	Vector3 cameraWorldPosition = activeCamera->GetPosition();
	Vector3 cameraForwardVector = activeCamera->GetWorldDirectionAtPixel(
		Vector2i{ (int)(x - viewportPosition.x), (int)(y - viewportPosition.y) });

	RaycastData raycastData;
	raycastData.from = cameraWorldPosition;
	raycastData.to = cameraWorldPosition + cameraForwardVector * 1000.f;
	raycastData.collisionGroup = CollisionGroup::All;
	raycastData.collisionMask = CollisionMask(0xFFFFFF);

	RaycastSingleResult raycastResult;

	engine->GetPhysicsWorld()->RaycastClosest(raycastData, raycastResult);

	return raycastResult.hitPosition;
}

bool EditorHUD::InsertSceneReference(const std::string& scenePath, const Vector3& position)
{
	Scene* currentScene = engine->GetApplication()->GetMainScene();
	if (!currentScene)
	{
		return false;
	}

	if (!IsSceneAssetPath(context_, scenePath))
	{
		return false;
	}

	const std::string contentRelativeScenePath = ContentPathUtils::NormalizePath(EditorAssetPathUtils::ToContentRelativePath(scenePath));
	if (contentRelativeScenePath.empty())
	{
		return false;
	}

	const std::string currentScenePath = ContentPathUtils::NormalizePath(ContentPathUtils::ToContentRelativePath(currentScene->GetPath()));
	if (!currentScenePath.empty() && currentScenePath == contentRelativeScenePath)
	{
		return false;
	}

	SceneReference sceneReference;
	sceneReference.path = contentRelativeScenePath;
	sceneReference.relativePosition = position;

	const SceneContentCounts counts = CaptureSceneContentCounts(currentScene);
	if (!SceneParser::InsertSceneReference(currentScene, sceneReference))
	{
		return false;
	}

	InitializeSceneContentAddedAfter(currentScene, counts);
	engine->GetResourceManager()->InitializePendingMaterials();
	engine->InitializePendingObjectsAndComponents();

	context_->MarkSceneDirty("Scene reference added");
	return true;
}

void EditorHUD::OnDeleteInputPressed()
{
	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureKeyboard)
	{
		return;
	}

	switch (context_->selectedObjectType)
	{
	case EditorSelectionType::Object:
	{
		const std::vector<ObjectBase*> selectedObjects = context_->GetSelectedObjects();
		if (selectedObjects.empty())
		{
			return;
		}

		for (ObjectBase* selectedObject : selectedObjects)
		{
			engine->GetApplication()->GetMainScene()->RemoveObject(selectedObject);
			selectedObject->Destroy();
		}
		break;
	}
	case EditorSelectionType::DirectionalLight:
		delete ((DirectionalLight*)context_->selectedObject);
		break;
	case EditorSelectionType::PointLight:
		delete ((PointLight*)context_->selectedObject);
		break;
	case EditorSelectionType::SpotLight:
		delete ((SpotLight*)context_->selectedObject);
		break;
	case EditorSelectionType::None:
	default:
		return;
	}

	context_->ClearSelection();
	context_->MarkSceneDirty("Scene object deleted");
}

void EditorHUD::OnCloneInputPressed()
{
	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureKeyboard)
	{
		return;
	}

	CloneSelectedObject();
}

ObjectBase* EditorHUD::CloneSelectedObject()
{
	if (context_->selectedObjectType != EditorSelectionType::Object || !context_->selectedObject)
	{
		return nullptr;
	}

	ObjectBase* selectedObject = static_cast<ObjectBase*>(context_->selectedObject);
	ObjectBase* clonedObject = selectedObject->Clone();
	if (!clonedObject)
	{
		return nullptr;
	}

	if (ObjectBase* parentObject = selectedObject->GetParent())
	{
		clonedObject->SetParent(parentObject, SnappingRule::KeepWorldAll, false);
	}

	engine->GetApplication()->GetMainScene()->AddObject(clonedObject);
	context_->SetSelection(clonedObject, EditorSelectionType::Object);
	context_->MarkSceneDirty("Scene object duplicated");

	return clonedObject;
}

void EditorHUD::OnFocusInputPressed()
{
	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureKeyboard)
	{
		return;
	}

	Vector3 position = Vector3::ZeroVector;
	switch (context_->selectedObjectType)
	{
	case EditorSelectionType::Object:
		position = ((ObjectBase*)context_->selectedObject)->GetWorldPosition();
		break;
	case EditorSelectionType::DirectionalLight:
	case EditorSelectionType::PointLight:
	case EditorSelectionType::SpotLight:
		position = ((Light*)context_->selectedObject)->GetPosition();
		break;
	case EditorSelectionType::None:
	default:
		return;
	}

	FocusToPosition(position);
}

void EditorHUD::OnCancelInputPressed()
{
	context_->ClearCreateData();
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
	context_->viewportCameraObject->SetWorldPosition(position - 20.f * context_->viewportCameraObject->GetForwardVector());
}

DirectionalLight* EditorHUD::CreateDirectionalLight()
{
	DirectionalLight* newDirectionalLight = new DirectionalLight();
	newDirectionalLight->SetIsShadowEnabled(true);
	context_->SetSelection(newDirectionalLight, EditorSelectionType::DirectionalLight);
	newDirectionalLight->PreInit();
	return newDirectionalLight;
}

PointLight* EditorHUD::CreatePointLight()
{
	PointLight* newPointLight = new PointLight();
	newPointLight->SetIsShadowEnabled(true);
	context_->SetSelection(newPointLight, EditorSelectionType::PointLight);
	newPointLight->PreInit();
	return newPointLight;
}

SpotLight* EditorHUD::CreateSpotLight()
{
	SpotLight* newSpotLight = new SpotLight();
	newSpotLight->SetIsShadowEnabled(true);
	context_->SetSelection(newSpotLight, EditorSelectionType::SpotLight);
	newSpotLight->PreInit();
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
	context_->SetSelection(newObjectBase, EditorSelectionType::Object);
	engine->GetApplication()->GetMainScene()->AddObject(newObjectBase);

	return newObjectBase;
}
