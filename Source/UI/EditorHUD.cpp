#include "EditorHUD.h"

#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Scene.h"

#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Goknar/Model/MeshUnit.h"

#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"

#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "Goknar/Helpers/SceneParser.h"

#include "Game.h"
#include "Objects/FreeCameraObject.h"
#include "Thirdparty/ImGuiOpenGL.h"

struct Folder
{
	std::string folderName;
	std::string fullPath;
	std::vector<Folder*> subFolders;
	std::vector<std::string> fileNames;
};

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

	uiImage_ = engine->GetResourceManager()->GetContent<Image>("Textures/UITexture.png");
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

	ImGui_DestroyFontsTexture();
	ImGui_DestroyDeviceObjects();
	ImGui_Shutdown();
	ImGui::DestroyContext(imguiContext_);
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

void EditorHUD::OnDeleteInputPressed()
{
	switch (selectedObjectType_)
	{
	case DetailObjectType::Object:
		((ObjectBase*)selectedObject_)->Destroy();
		break;
	case DetailObjectType::DirectionalLight:
		delete ((DirectionalLight*)selectedObject_);
		break;
	case DetailObjectType::PointLight:
		delete ((PointLight*)selectedObject_);
		break;
	case DetailObjectType::SpotLight:
		delete ((SpotLight*)selectedObject_);
		break;
	case DetailObjectType::None:
	default:
		return;
	}

	selectedObjectType_ = DetailObjectType::None;
	selectedObject_ = nullptr;
}

void EditorHUD::OnFocusInputPressed()
{
	Vector3 position = Vector3::ZeroVector;
	switch (selectedObjectType_)
	{
	case DetailObjectType::Object:
		position = ((ObjectBase*)selectedObject_)->GetWorldPosition();
		break;
	case DetailObjectType::DirectionalLight:
	case DetailObjectType::PointLight:
	case DetailObjectType::SpotLight:
		position = ((Light*)selectedObject_)->GetPosition();
		break;
	case DetailObjectType::None:
	default:
		return;
	}

	FocusToPosition(position);
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
				windowOpenMap_["Scene save path"] = true;
			}

			if (ImGui::MenuItem("Save scene"))
			{
				if (sceneSavePath_.empty())
				{
					windowOpenMap_["Scene save path"] = true;
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
	DrawSceneWindow();
	DrawObjectsWindow();
	DrawFileBrowserWindow();
	DrawDetailsWindow();

	if (windowOpenMap_["Scene save path"])
	{
		OnSavePathEmpty();
	}
}

void EditorHUD::DrawCameraInfo()
{
	BeginTransparentWindow("CameraInfo");

	FreeCameraObject* freeCameraObject = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject();
	ImGui::Text((std::string("Position: ") + freeCameraObject->GetWorldPosition().ToString()).c_str());
	ImGui::Text((std::string("Rotation: ") + freeCameraObject->GetWorldRotation().ToEulerDegrees().ToString()).c_str());
	ImGui::Text((std::string("Forward Vector: ") + freeCameraObject->GetForwardVector().ToString()).c_str());

	EndWindow();
}

void EditorHUD::DrawSceneWindow()
{
	BeginWindow("Scene", dockableWindowFlags_);
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
					selectedObjectType_ = DetailObjectType::DirectionalLight;
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
					selectedObjectType_ = DetailObjectType::PointLight;
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
					selectedObjectType_ = DetailObjectType::SpotLight;
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
			if (object->GetName().find("__Editor__") != std::string::npos)
			{
				continue;
			}

			if (ImGui::Selectable(object->GetName().c_str(), selectedObject_ == object, ImGuiSelectableFlags_AllowDoubleClick))
			{
				selectedObject_ = object;
				selectedObjectType_ = DetailObjectType::Object;

				if (ImGui::IsMouseDoubleClicked(ImGuiButtonFlags_MouseButtonLeft))
				{
					FreeCameraObject* freeCameraObject = dynamic_cast<Game*>(engine->GetApplication())->GetFreeCameraObject();
					freeCameraObject->SetWorldPosition(object->GetWorldPosition() - 20.f * freeCameraObject->GetForwardVector());
				}
			}
		}

		ImGui::TreePop();
	}
}

void EditorHUD::BuildAssetTree(Folder* folder)
{
	for (auto subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->folderName.c_str()))
		{
			BuildAssetTree(subFolder);
			ImGui::TreePop();
		}
	}
	for (auto fileName : folder->fileNames)
	{
		ImGui::Text(fileName.c_str());
	}
}

void EditorHUD::DrawObjectsWindow()
{
	BeginWindow("Objects", dockableWindowFlags_);
	if (ImGui::TreeNode("Lights"))
	{
		if (ImGui::Button("Directional Light"))
		{
			DirectionalLight* newDirectionalLight = new DirectionalLight();
		}
		else if (ImGui::Button("Point Light"))
		{
			PointLight* newPointLight = new PointLight();
		}
		else if (ImGui::Button("Spot Light"))
		{
			SpotLight* newSpotLight = new SpotLight();
		}

		ImGui::TreePop();
	}
	ImGui::Separator();
	EndWindow();
}

void EditorHUD::DrawFileBrowserWindow()
{
	BeginWindow("FileBrowser", dockableWindowFlags_);

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

	Folder* rootFolder = new Folder();
	rootFolder->folderName = "Content";

	std::map<std::string, Folder*> folderMap;
	int contentPathsSize = contentPaths.size();
	for (size_t pathIndex = 0; pathIndex < contentPathsSize; pathIndex++)
	{
		std::string currentPath = contentPaths[pathIndex];

		Folder* parentFolder = rootFolder;

		bool breakTheLoop = false;
		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));

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
				if (folderMap.find(folderName) == folderMap.end())
				{
					folderMap[folderName] = new Folder();
					folderMap[folderName]->folderName = folderName;
					folderMap[folderName]->fullPath = currentPath;
				}

				if (std::find(parentFolder->subFolders.begin(),
					parentFolder->subFolders.end(),
					folderMap[folderName]) == std::end(parentFolder->subFolders))
				{
					parentFolder->subFolders.push_back(folderMap[folderName]);
				}
			}

			if (breakTheLoop)
			{
				break;
			}

			parentFolder = folderMap[folderName];
			currentPath = currentPath.substr(folderName.size() + 1);
		}
	}

	if (ImGui::TreeNode("Assets"))
	{
		BuildAssetTree(rootFolder);
		ImGui::TreePop();
	}

	for (auto folder : folderMap)
	{
		delete folder.second;
	}

	delete rootFolder;

	EndWindow();
}

void EditorHUD::DrawDetailsWindow()
{
	BeginWindow("Details", dockableWindowFlags_);

	switch (selectedObjectType_)
	{
	case DetailObjectType::None:
		break;
	case DetailObjectType::Object:
		DrawDetailsWindow_Object();
		ImGui::Separator();
		break;
	case DetailObjectType::DirectionalLight:
		DrawDetailsWindow_DirectionalLight();
		ImGui::Separator();
		break;
	case DetailObjectType::PointLight:
		DrawDetailsWindow_PointLight();
		ImGui::Separator();
		break;
	case DetailObjectType::SpotLight:
		DrawDetailsWindow_SpotLight();
		ImGui::Separator();
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

	ImGui::PushItemWidth(100.f);

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
	DrawInputText("##Position", selectedObjectWorldPosition);
	selectedObject->SetWorldPosition(selectedObjectWorldPosition);

	ImGui::Text("Rotation: ");
	ImGui::SameLine();
	DrawInputText("##Rotation", selectedObjectWorldRotationEulerDegrees);
	selectedObject->SetWorldRotation(Quaternion::FromEulerDegrees(selectedObjectWorldRotationEulerDegrees));

	ImGui::Text("Scaling: ");
	ImGui::SameLine();
	DrawInputText("##Scaling", selectedObjectWorldScaling);
	selectedObject->SetWorldScaling(selectedObjectWorldScaling);

	ImGui::PopItemWidth();
}

void EditorHUD::DrawDetailsWindow_DirectionalLight()
{
	DirectionalLight* light = static_cast<DirectionalLight*>(selectedObject_);

	Vector3 lightPosition = light->GetPosition();
	Vector3 lightDirection = light->GetDirection();
	Vector3 lightColor = light->GetColor();
	float lightIntensity = light->GetIntensity();
	float lightShadowIntensity = light->GetShadowIntensity();
	bool lightIsShadowEnabled = light->GetIsShadowEnabled();

	ImGui::PushItemWidth(100.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputText("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Direction: ");
	ImGui::SameLine();
	DrawInputText("##Direction", lightDirection);
	light->SetDirection(lightDirection);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	DrawInputText("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	DrawInputText("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	DrawCheckbox("##IsCastingShadow", lightIsShadowEnabled);
	ImGui::PopItemFlag();
	ImGui::PopStyleVar();

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	DrawInputText("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	light->SetIsShadowEnabled(lightIsShadowEnabled);

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

	ImGui::PushItemWidth(100.f);

	ImGui::Text("Position: ");
	ImGui::SameLine();
	DrawInputText("##Position", lightPosition);
	light->SetPosition(lightPosition);

	ImGui::Text("Intensity: ");
	ImGui::SameLine();
	DrawInputText("##Intensity", lightIntensity);
	light->SetIntensity(lightIntensity);

	ImGui::Text("Color: ");
	ImGui::SameLine();
	DrawInputText("##Color", lightColor);
	light->SetColor(lightColor);

	ImGui::Text("Radius: ");
	ImGui::SameLine();
	DrawInputText("##Radius", lightRadius);
	light->SetRadius(lightRadius);

	ImGui::Text("Cast shadow: ");
	ImGui::SameLine();
	ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	DrawCheckbox("##IsCastingShadow", lightIsShadowEnabled);
	ImGui::PopItemFlag();
	ImGui::PopStyleVar();
	light->SetIsShadowEnabled(lightIsShadowEnabled);

	ImGui::Text("Shadow Intensity: ");
	ImGui::SameLine();
	DrawInputText("##Shadow Intensity", lightShadowIntensity);
	light->SetShadowIntensity(lightShadowIntensity);

	ImGui::PopItemWidth();
}

void EditorHUD::DrawDetailsWindow_SpotLight()
{

}

void EditorHUD::DrawInputText(const std::string& name, std::string& value)
{
	char* valueChar = const_cast<char*>(value.c_str());
	ImGui::InputText(name.c_str(), valueChar, 64);
	value = std::string(valueChar);
}

void EditorHUD::DrawInputText(const std::string& name, float& value)
{
	char valueChar[64];
	sprintf(valueChar, "%.4f", value);
	ImGui::InputText((name + "X").c_str(), valueChar, 64);
	float newValue = (float)atof(valueChar);
	if (SMALLER_EPSILON < GoknarMath::Abs(value - newValue))
	{
		value = newValue;
	}
}

void EditorHUD::DrawInputText(const std::string& name, Vector3& vector)
{
	char valueCharX[64];
	sprintf(valueCharX, "%.4f", vector.x);
	ImGui::InputText((name + "X").c_str(), valueCharX, 64);
	float newValueX = (float)atof(valueCharX);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.x - newValueX))
	{
		vector.x = newValueX;
	}

	ImGui::SameLine();

	char valueCharY[64];
	sprintf(valueCharY, "%.4f", vector.y);
	ImGui::InputText((name + "Y").c_str(), valueCharY, 64);
	float newValueY = (float)atof(valueCharY);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.y - newValueY))
	{
		vector.y = newValueY;
	}

	ImGui::SameLine();

	char valueCharZ[64];
	sprintf(valueCharZ, "%.4f", vector.z);
	ImGui::InputText((name + "Z").c_str(), valueCharZ, 64);
	float newValueZ = (float)atof(valueCharZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.z - newValueZ))
	{
		vector.z = newValueZ;
	}
}

void EditorHUD::DrawInputText(const  std::string& name, Quaternion& quaternion)
{
	char valueX[64];
	sprintf(valueX, "%.4f", quaternion.x);
	ImGui::InputText((name + "X").c_str(), valueX, 64);
	float newQuaternionX = (float)atof(valueX);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.x - newQuaternionX))
	{
		quaternion.x = newQuaternionX;
	}

	ImGui::SameLine();

	char valueCharY[64];
	sprintf(valueCharY, "%.4f", quaternion.y);
	ImGui::InputText((name + "Y").c_str(), valueCharY, 64);
	float newQuaternionY = (float)atof(valueCharY);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.y - newQuaternionY))
	{
		quaternion.y = newQuaternionY;
	}

	ImGui::SameLine();

	char valueCharZ[64];
	sprintf(valueCharZ, "%.4f", quaternion.z);
	ImGui::InputText((name + "Z").c_str(), valueCharZ, 64);
	float newQuaternionZ = (float)atof(valueCharZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.z - newQuaternionZ))
	{
		quaternion.z = newQuaternionZ;
	}

	ImGui::SameLine();

	char valueW[64];
	sprintf(valueW, "%.4f", quaternion.w);
	ImGui::InputText((name + "W").c_str(), valueW, 64);
	float newQuaternionW = (float)atof(valueW);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.w - newQuaternionW))
	{
		quaternion.w = newQuaternionW;
	}
}

void EditorHUD::DrawCheckbox(const std::string& name, bool& value)
{
	ImGui::Checkbox(name.c_str(), &value);
}

void EditorHUD::BeginWindow(const std::string& name, ImGuiWindowFlags flags)
{
	if (windowOpenMap_.find(name) == windowOpenMap_.end())
	{
		windowOpenMap_[name] = true;
	}

	ImGui::Begin(name.c_str(), &windowOpenMap_[name], flags);
}

void EditorHUD::BeginTransparentWindow(const std::string& name)
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;
	windowFlags |= ImGuiWindowFlags_NoTitleBar;

	BeginWindow(name, windowFlags);
}

void EditorHUD::EndWindow()
{
	ImGui::End();
}

bool EditorHUD::BeginDialogWindow_OneTextBoxOneButton(const std::string& windowTitle, const std::string& text, const std::string& currentValue, const std::string& buttonText)
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

	BeginWindow(windowTitle, windowFlags);

	ImGui::Text(text.c_str());
	ImGui::SameLine();

	char* input = const_cast<char*>(currentValue.c_str());
	ImGui::InputText((windowTitle + "_TextBox").c_str(), input, 64);

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

void EditorHUD::OnSavePathEmpty()
{
	std::string windowName = "##Scene save path";
	if (BeginDialogWindow_OneTextBoxOneButton(windowName, "Path: ", sceneSavePath_, "Save"))
	{
		SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + sceneSavePath_);

		windowOpenMap_[windowName] = false;
	}
}
