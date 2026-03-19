#include "MeshViewerCameraController.h"

#include "Goknar/Application.h"
#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Geometry/Box.h"

#include "Objects/MeshViewerCameraObject.h"

MeshViewerCameraController::MeshViewerCameraController(MeshViewerCameraObject* cameraObject) :
	cameraObject_(cameraObject)
{
	onMouseRightClickPressedDelegate_ = Delegate<void()>::Create<MeshViewerCameraController, &MeshViewerCameraController::OnMouseRightClickPressed>(this);
	onMouseRightClickReleasedDelegate_ = Delegate<void()>::Create<MeshViewerCameraController, &MeshViewerCameraController::OnMouseRightClickReleased >(this);
	onMouseMiddleClickPressedDelegate_ = Delegate<void()>::Create<MeshViewerCameraController, &MeshViewerCameraController::OnMouseMiddleClickPressed>(this);
	onMouseMiddleClickReleasedDelegate_ = Delegate<void()>::Create<MeshViewerCameraController, &MeshViewerCameraController::OnMouseMiddleClickReleased>(this);

	onScrollMoveDelegate_ = Delegate<void(double, double)>::Create<MeshViewerCameraController, &MeshViewerCameraController::ScrollListener>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::Create<MeshViewerCameraController, &MeshViewerCameraController::CursorMovement>(this);

	SetName("__Editor__MeshViewerCameraController");
}

MeshViewerCameraController::~MeshViewerCameraController()
{
	UnbindInputDelegates();
}

void MeshViewerCameraController::BeginGame()
{
	ResetView();
}

void MeshViewerCameraController::SetIsActive(bool isActive)
{
	if (GetIsActive() == isActive)
	{
		return;
	}

	Controller::SetIsActive(isActive);

	if (isActive)
	{
		BindInputDelegates();
	}
	else
	{
		UnbindInputDelegates();
	}
}

void MeshViewerCameraController::ResetView()
{
	orbitCenter_ = objectPositionOffset_;
	orbitDistance_ = 10.f;
	pitch_ = 0.f;
	yaw_ = 0.f;
	UpdateCameraTransform();
}

void MeshViewerCameraController::ResetViewWithBoundingBox(ObjectBase* object, const Box& aabb)
{
	// 1. Calculate the center and radius of the mesh's bounding box
	Vector3 center = (aabb.GetMin() + aabb.GetMax()) * 0.5f;
	float radius = aabb.GetSize().Length() * 0.5f;

	// 2. Extract frustum data from the camera
	Camera* camera = cameraObject_->GetCameraComponent()->GetCamera();
	const Vector4& nearPlane = camera->GetNearPlane(); // Left, Right, Bottom, Top
	float right = nearPlane.y;
	float top = nearPlane.w;
	float nearDist = camera->GetNearDistance();

	// 3. Calculate horizontal and vertical half-FOVs
	float fovXHalfRad = std::atan2(right, nearDist);
	float fovYHalfRad = std::atan2(top, nearDist);

	// 4. Calculate required distance to fit the bounding radius on both axes
	float distX = radius / std::sin(fovXHalfRad);
	float distY = radius / std::sin(fovYHalfRad);

	// Use the larger distance to ensure it fits completely, adding a 20% margin for padding
	float distance = GoknarMath::Max(distX, distY) * 1.2f;

	object->SetWorldPosition(objectPositionOffset_ - center);
	SetOrbitDistance(distance);
	UpdateCameraTransform();
}

void MeshViewerCameraController::CursorMovement(double x, double y)
{
	Vector2 currentCursorPosition(x, y);
	Vector2 delta = currentCursorPosition - previousCursorPosition_;

	if (isOrbiting_)
	{
		yaw_ -= delta.x * orbitSpeed_;
		pitch_ -= delta.y * orbitSpeed_;
		
		pitch_ = GoknarMath::Clamp(pitch_, -HALF_PI + 0.01f, HALF_PI - 0.01f);
		
		UpdateCameraTransform();
	}
	else if (isPanning_)
	{
		Vector3 right = cameraObject_->GetLeftVector() * -1.f;
		Vector3 up = cameraObject_->GetUpVector();
		
		orbitCenter_ += (right * -delta.x * panSpeed_) + (up * delta.y * panSpeed_);
		UpdateCameraTransform();
	}

	previousCursorPosition_ = currentCursorPosition;
}

void MeshViewerCameraController::ScrollListener(double x, double y)
{
	if (!GetIsActive())
	{
		return;
	}

	orbitDistance_ -= (float)y * zoomSpeed_;
	orbitDistance_ = GoknarMath::Max(orbitDistance_, 0.1f);
	UpdateCameraTransform();
}

void MeshViewerCameraController::UpdateCameraTransform()
{
	Vector3 eulerAngles(0.f, pitch_, yaw_);
	cameraObject_->SetWorldRotation(Quaternion::FromEulerRadians(eulerAngles));

	Vector3 forward = cameraObject_->GetForwardVector();

	Vector3 newPosition = orbitCenter_ - (forward * orbitDistance_);
	cameraObject_->SetWorldPosition(newPosition);
}

void MeshViewerCameraController::OnMouseRightClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(x, y);
	previousCursorPosition_ = Vector2(x, y);
	isOrbiting_ = true;
}

void MeshViewerCameraController::OnMouseRightClickReleased()
{
	isOrbiting_ = false;
}

void MeshViewerCameraController::OnMouseMiddleClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(x, y);
	previousCursorPosition_ = Vector2(x, y);
	isPanning_ = true;
}

void MeshViewerCameraController::OnMouseMiddleClickReleased()
{
	isPanning_ = false;
}

void MeshViewerCameraController::BindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);
	inputManager->AddScrollDelegate(onScrollMoveDelegate_);
	inputManager->AddCursorDelegate(onCursorMoveDelegate_);
}

void MeshViewerCameraController::UnbindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);
	
	onMouseRightClickReleasedDelegate_();
	onMouseMiddleClickReleasedDelegate_();

	inputManager->RemoveScrollDelegate(onScrollMoveDelegate_);
	inputManager->RemoveCursorDelegate(onCursorMoveDelegate_);
}