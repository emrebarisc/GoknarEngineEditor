#pragma once

#include <unordered_map>

#include "Goknar/Core.h"
#include "Goknar/Delegates/Delegate.h"
#include "Goknar/UI/HUD.h"

#include "imgui.h"

class ImGuiContext;

class Image;
class ObjectBase;
class PhysicsObject;

enum class DetailObjectType
{
	None = 0,
	Object,
	DirectionalLight,
	PointLight,
	SpotLight
};

class GOKNAR_API EditorHUD : public HUD
{
public:
	EditorHUD();
	virtual ~EditorHUD();

    virtual void PreInit() override;
    virtual void Init() override;
    virtual void PostInit() override;

    virtual void BeginGame() override;

    virtual void UpdateHUD() override;

protected:

private:
	struct Folder
	{
		std::string folderName;
		std::string fullPath;
		std::vector<Folder*> subFolders;
		std::vector<std::string> fileNames;
	};

	Folder* rootFolder;
	std::unordered_map<std::string, Folder*> folderMap;

	void OnKeyboardEvent(int key, int scanCode, int action, int mod);

	void OnCursorMove(double xPosition, double yPosition);
	void OnScroll(double xOffset, double yOffset);
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void OnCharPressed(unsigned int codePoint);
	void OnWindowSizeChanged(int width, int height);

	void OnDeleteInputPressed();
	void OnFocusInputPressed();

	void DrawEditorHUD();

	void DrawCameraInfo();
	void DrawGameOptionsBar();

	void DrawSceneWindow();
	void DrawSceneLights();
	void DrawSceneObjects();

	void DrawObjectsWindow();
	void BuildFileTree();
	void DrawFileTree(Folder* folder);
	void DrawFileGrid(Folder* folder, std::string& selectedFileName, bool& isAFileSelected);

	void DrawFileBrowserWindow();
	void DrawDetailsWindow();
	void DrawDetailsWindow_Object();
	void DrawDetailsWindow_AddComponentOptions(ObjectBase* object);
	void DrawDetailsWindow_Component(Component* component);
	void DrawDetailsWindow_DirectionalLight();
	void DrawDetailsWindow_PointLight();
	void DrawDetailsWindow_SpotLight();

	void DrawInputText(const std::string& name, std::string& value);
	void DrawInputText(const std::string& name, float& value);
	void DrawInputText(const std::string& name, Vector3& vector);
	void DrawInputText(const std::string& name, Quaternion& quaternion);

	void DrawCheckbox(const std::string& name, bool& value);

	bool DrawAssetSelector(std::string& selectedPath);

	void BeginWindow(const std::string& name, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
	void BeginTransparentWindow(const std::string& name);
	void EndWindow();

	bool BeginDialogWindow_OneTextBoxOneButton(
		const std::string& windowTitle, const std::string& text, 
		const std::string& currentValue, const std::string& buttonText,
		ImGuiWindowFlags flags);

	void FocusToPosition(const Vector3& position);
	void OpenSaveSceneDialog();

	Delegate<void(int, int, int, int)> onKeyboardEventDelegate_;
	Delegate<void(double, double)> onCursorMoveDelegate_;
	Delegate<void(double, double)> onScrollDelegate_;
	Delegate<void()> onLeftClickPressedDelegate_;
	Delegate<void()> onLeftClickReleasedDelegate_;
	Delegate<void(unsigned int)> onCharPressedDelegate_;

	Delegate<void(int, int)> onWindowSizeChangedDelegate_;

	Delegate<void()> onDeleteInputPressedDelegate_;
	Delegate<void()> onFocusInputPressedDelegate_;

	Vector2i windowSize_;

	ImVec2 buttonSize_{ 100.f, 40.f };

	std::unordered_map<std::string, std::function<void(ObjectBase*)>> objectBaseReflections;
	std::unordered_map<std::string, std::function<void(PhysicsObject*)>> physicsObjectReflections;

	Image* uiImage_;
	ImGuiContext* imguiContext_;

	DetailObjectType selectedObjectType_{ DetailObjectType::None };
	void* selectedObject_{ nullptr };

	std::string sceneSavePath_{};
	std::string saveSceneDialogWindowName_{ "Save Scene" };
	std::string sceneWindowName_{ "Scene" };
	std::string objectsWindowName_{ "Objects" };
	std::string fileBrowserWindowName_{ "FileBrowser" };
	std::string detailsWindowName_{ "Details" };
	std::string gameOptionsWindowName_{ "GameOptions" };

	std::unordered_map<std::string, bool> windowOpenMap_;

	ImGuiWindowFlags dockableWindowFlags_{ ImGuiWindowFlags_None };

	bool drawCollisionWorld_{ false };
};