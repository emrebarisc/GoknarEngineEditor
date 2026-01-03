#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Goknar/Core.h"
#include "Goknar/Delegates/Delegate.h"
#include "Goknar/UI/HUD.h"

#include "Panels/EditorPanel.h"

#include "imgui.h"

class ImGuiContext;

class DirectionalLight;
class PointLight;
class SpotLight;

class Image;
class ObjectBase;
class PhysicsObject;

class BoxCollisionComponent;
class Camera;
class CapsuleCollisionComponent;
class FrameBuffer;
class StaticMeshComponent;
class SphereCollisionComponent;
class MovingTriangleMeshCollisionComponent;
class NonMovingTriangleMeshCollisionComponent;
class RenderTarget;

class GOKNAR_API EditorHUD : public HUD
{
public:
	EditorHUD();
	EditorHUD(const EditorHUD&) = delete;
	EditorHUD(EditorHUD&&) = default;
	virtual ~EditorHUD();

	EditorHUD& operator=(const EditorHUD&) = delete;
	EditorHUD& operator=(EditorHUD&&) = default;

    virtual void PreInit() override;
    virtual void Init() override;
    virtual void PostInit() override;

    virtual void BeginGame() override;

	virtual void SetupDocking();

    virtual void UpdateHUD() override;

	template<typename T>
	void ShowPanel();

	template<typename T>
	void HidePanel();

	const std::vector<std::unique_ptr<IEditorPanel>>& GetPanels() const
	{
		return panels_;
	}

protected:

private:
	void OnKeyboardEvent(int key, int scanCode, int action, int mod);

	void OnCursorMove(double xPosition, double yPosition);
	void OnScroll(double xOffset, double yOffset);
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void OnCharPressed(unsigned int codePoint);
	void OnWindowSizeChanged(int width, int height);

	void OnDeleteInputPressed();
	void OnFocusInputPressed();
	void OnCancelInputPressed();

	void DrawEditorHUD();

	void DrawDrawInfo();
	void DrawGameOptionsBar();

	void DrawObjectsWindow();

	void DrawCheckbox(const std::string& name, bool& value);

	//bool DrawAssetSelector(std::string& selectedPath);

	void BeginWindow(const std::string& name, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
	void BeginTransparentWindow(const std::string& name, ImGuiWindowFlags additionalFlags = ImGuiWindowFlags_None);
	void EndWindow();



	void FocusToPosition(const Vector3& position);

	DirectionalLight* CreateDirectionalLight();
	PointLight* CreatePointLight();
	SpotLight* CreateSpotLight();
	ObjectBase* CreateObject(const std::string& typeName);

	void OnPlaceObject();
	Vector3 RaycastWorld();
	void DrawObjectNameToCreateWindow();


	Delegate<void(int, int, int, int)> onKeyboardEventDelegate_;
	Delegate<void(double, double)> onCursorMoveDelegate_;
	Delegate<void(double, double)> onScrollDelegate_;
	Delegate<void()> onLeftClickPressedDelegate_;
	Delegate<void()> onLeftClickReleasedDelegate_;
	Delegate<void(unsigned int)> onCharPressedDelegate_;

	Delegate<void(int, int)> onWindowSizeChangedDelegate_;

	Delegate<void()> onDeleteInputPressedDelegate_;
	Delegate<void()> onFocusInputPressedDelegate_;
	Delegate<void()> onCancelInputPressedDelegate_;

	Vector2i windowSize_;

	ImVec2 buttonSize_{ 100.f, 40.f };

	ImVec2 viewportSize_{ 0.f, 0.f };
	ImVec2 viewportPosition_{ 0.f, 0.f };

	std::unordered_map<std::string, std::function<void(ObjectBase*)>> objectBaseReflections;
	std::unordered_map<std::string, std::function<void(PhysicsObject*)>> physicsObjectReflections;

	Image* uiImage_;

	std::unordered_map<std::string, bool> windowOpenMap_;

	ImGuiWindowFlags dockableWindowFlags_{ ImGuiWindowFlags_None };

	bool drawCollisionWorld_{ false }; 
	bool shouldFreeCameraControllerBeEnabled_{ true };


	template<typename T>
	void AddPanel();

	std::vector<std::unique_ptr<IEditorPanel>> panels_;
	std::unordered_map<std::string, int> panelIndexMap_;
};

template<typename T>
inline void EditorHUD::AddPanel()
{
	panels_.emplace_back(std::make_unique<T>(this));
	panelIndexMap_[typeid(T).name()] = panels_.size() - 1;
}

template<typename T>
inline void EditorHUD::ShowPanel()
{
	panels_[panelIndexMap_[typeid(T).name()]]->SetIsOpen(true);
}

template<typename T>
inline void EditorHUD::HidePanel()
{
	panels_[panelIndexMap_[typeid(T).name()]]->SetIsOpen(false);
}
