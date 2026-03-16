#pragma once

#include "Goknar/Controller.h"
#include "Goknar/Delegates/Delegate.h"
#include "Goknar/Math/GoknarMath.h"

class MeshViewerCameraObject;

class MeshViewerCameraController : public Controller
{
public:
	MeshViewerCameraController(MeshViewerCameraObject* cameraObject);
	~MeshViewerCameraController();

	virtual void BeginGame() override;
	virtual void SetIsActive(bool isActive) override;

	void ResetView();

protected:
	void SetupInputDelegates() override {}

private:
	void CursorMovement(double x, double y);
	void ScrollListener(double x, double y);

	void OnMouseRightClickPressed();
	void OnMouseRightClickReleased();
	void OnMouseMiddleClickPressed();
	void OnMouseMiddleClickReleased();

	void UpdateCameraTransform();

	void BindInputDelegates();
	void UnbindInputDelegates();

	Delegate<void()> onMouseRightClickPressedDelegate_;
	Delegate<void()> onMouseRightClickReleasedDelegate_;
	Delegate<void()> onMouseMiddleClickPressedDelegate_;
	Delegate<void()> onMouseMiddleClickReleasedDelegate_;

	Delegate<void(double, double)> onScrollMoveDelegate_;
	Delegate<void(double, double)> onCursorMoveDelegate_;

	Vector2 previousCursorPosition_;

	MeshViewerCameraObject* cameraObject_;
	
	bool isOrbiting_{ false };
	bool isPanning_{ false };

	Vector3 orbitCenter_{ Vector3::ZeroVector };
	float orbitDistance_{ 10.f };
	float pitch_{ 0.f };
	float yaw_{ 0.f };

	float panSpeed_{ 0.02f };
	float orbitSpeed_{ 0.01f };
	float zoomSpeed_{ 0.5f };
};