#include "EditorHUD.h"

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

#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"

#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"

#include "Game.h"
#include "Thirdparty/ImGuiOpenGL.h"

struct Folder
{
	std::string folderName;
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

	uiImage_ = engine->GetResourceManager()->GetContent<Image>("Textures/UITexture.png");
}

EditorHUD::~EditorHUD()
{
	engine->GetInputManager()->RemoveKeyboardListener(onKeyboardEventDelegate_);

	engine->GetInputManager()->RemoveCursorDelegate(onCursorMoveDelegate_);
	engine->GetInputManager()->RemoveScrollDelegate(onScrollDelegate_);

	engine->GetInputManager()->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_PRESS, onLeftClickPressedDelegate_);
	engine->GetInputManager()->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_RELEASE, onLeftClickReleasedDelegate_);

	engine->GetInputManager()->RemoveCharDelegate(onCharPressedDelegate_);

	ImGui_DestroyFontsTexture();
	ImGui_DestroyDeviceObjects();
	ImGui_Shutdown();
	ImGui::DestroyContext(imguiContext_);
}

void EditorHUD::PreInit()
{
	HUD::PreInit();

	engine->GetInputManager()->AddKeyboardListener(onKeyboardEventDelegate_);

	engine->GetInputManager()->AddCursorDelegate(onCursorMoveDelegate_);
	engine->GetInputManager()->AddScrollDelegate(onScrollDelegate_);

	engine->GetInputManager()->AddMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_PRESS, onLeftClickPressedDelegate_);
	engine->GetInputManager()->AddMouseInputDelegate(MOUSE_MAP::BUTTON_1, INPUT_ACTION::G_RELEASE, onLeftClickReleasedDelegate_);

	engine->GetInputManager()->AddCharDelegate(onCharPressedDelegate_);
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
	
	engine->SetTimeScale(0.f);
    
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
    ImGuiStyle* style = &ImGui::GetStyle();

	SetupFullscreenWindow();
	
	DrawEditorHUD();

	ImGui::Render();
	ImGui_RenderDrawData(ImGui::GetDrawData());
}

void EditorHUD::SetupFullscreenWindow()
{
	ImVec2 windowSize = ImVec2{(float)windowSize_.x, (float)windowSize_.y};
	ImGui::SetNextWindowSize(windowSize);
	ImVec2 windowPosition = ImVec2{0.f, 0.f};//ImVec2{(float)windowSize.x * 0.5f, (float)windowSize.y * 0.5f};
	ImGui::SetNextWindowPos(windowPosition);
}

void EditorHUD::DrawEditorHUD()
{
	static bool p_open = true;
	static bool opt_fullscreen = true;
	static bool opt_padding = false;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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
	ImGui::Begin("DockSpace Demo", &p_open, window_flags);
	if (!opt_padding)
		ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	// Submit the DockSpace
	ImGuiIO& io = ImGui::GetIO(); 

	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
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

	DrawSceneWindow();
	DrawObjectsWindow();
	DrawFileBrowserWindow();
	DrawDetailsWindow();
}

void EditorHUD::DrawSceneWindow()
{
	BeginWindow("Scene");
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
				if (ImGui::Button(directionalLight->GetName().c_str()))
				{

				}
			}
		}
		if (0 < pointLightCount)
		{
			for (int lightIndex = 0; lightIndex < pointLightCount; lightIndex++)
			{
				PointLight* pointLight = pointLights[lightIndex];
				if (ImGui::Button(pointLight->GetName().c_str()))
				{

				}
			}
		}
		if (0 < spotLightCount)
		{
			for (int lightIndex = 0; lightIndex < spotLightCount; lightIndex++)
			{
				SpotLight* spotLight = spotLights[lightIndex];
				if (ImGui::Button(spotLight->GetName().c_str()))
				{

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

			if (ImGui::Button(object->GetName().c_str()))
			{
				Camera* camera = engine->GetCameraManager()->GetActiveCamera();
				engine->GetCameraManager()->GetActiveCamera()->SetPosition(camera->GetPosition() - 10.f * camera->GetForwardVector());
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
	BeginWindow("Objects");
	if (ImGui::TreeNode("Lights"))
	{
		if (ImGui::Button("Directional Light"))
		{
			new DirectionalLight();
		}
		else if (ImGui::Button("Point Light"))
		{
			new PointLight();
		}
		else if (ImGui::Button("Spot Light"))
		{
			new SpotLight();
		}

		ImGui::TreePop();
	}
	EndWindow();
}

void EditorHUD::DrawFileBrowserWindow()
{
	BeginWindow("FileBrowser");

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
	BeginWindow("Details");
	EndWindow();
}

void EditorHUD::BeginWindow(const std::string name)
{
    ImGuiWindowFlags windowFlags = 
        //ImGuiWindowFlags_NoTitleBar          | 
        //ImGuiWindowFlags_NoResize            |
        //ImGuiWindowFlags_NoMove              |
        //ImGuiWindowFlags_NoScrollbar		 |
        //ImGuiWindowFlags_NoSavedSettings     |
        0;

	bool isOpen = true;
	ImGui::Begin(name.c_str(), &isOpen, windowFlags);
}

void EditorHUD::EndWindow()
{
	ImGui::End();
}