#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Goknar/Core.h"
#include "Goknar/GoknarAssert.h"
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

class EditorContext;

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

	virtual void DrawBackgroundWindow();

    virtual void UpdateHUD() override;

	template<typename T>
	void ShowPanel();

	template<typename T>
	void HidePanel();

	const std::vector<std::unique_ptr<IEditorPanel>>& GetPanels() const
	{
		return panels_;
	}

	template<typename T>
	IEditorPanel* GetPanel() const
	{
		std::string mapKey = std::string(typeid(T).name());
		auto it = panelIndexMap_.find(mapKey);

		if (it == panelIndexMap_.end())
		{
			return nullptr;
		}

		return panels_[panelIndexMap_.at(mapKey)].get();
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

	void DrawGameOptionsBar();

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

	Image* uiImage_;

	std::unordered_map<std::string, bool> windowOpenMap_;

	template<typename T>
	void AddPanel();

	std::vector<std::unique_ptr<IEditorPanel>> panels_;
	std::unordered_map<std::string, int> panelIndexMap_;

	EditorContext* context_;
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
	std::string mapKey = typeid(T).name();
	GOKNAR_ASSERT(panelIndexMap_.find(mapKey) != panelIndexMap_.end());
	
	panels_[panelIndexMap_[typeid(T).name()]]->SetIsOpen(true);
}

template<typename T>
inline void EditorHUD::HidePanel()
{
	std::string mapKey = typeid(T).name();
	GOKNAR_ASSERT(panelIndexMap_.find(mapKey) != panelIndexMap_.end());

	panels_[panelIndexMap_[typeid(T).name()]]->SetIsOpen(false);
}
