#include "EditorHUD.h"

#include "imgui/imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Editor/ImGuiEditor/ImGuiOpenGL.h"

#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"

#include "Game.h"

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
	
	ImGui_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)windowSize_.x, (float)windowSize_.y);

	ImGui::StyleColorsDark();
    ImGuiStyle* style = &ImGui::GetStyle();

	SetupFullscreenWindow();
	BeginWindow();
	
	DrawEditorHUD();

	EndWindow();

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

}

void EditorHUD::BeginWindow()
{
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoTitleBar          | 
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoScrollbar		 |
        ImGuiWindowFlags_NoSavedSettings     |
        0;

	bool isOpen = true;
	ImGui::Begin("Window", &isOpen, windowFlags);
}

void EditorHUD::EndWindow()
{
	ImGui::End();
}