#include "pch.h"

#include "FreeCameraController.h"

#include "Goknar/Scene.h"

#include "Goknar/Application.h"
#include "Goknar/Camera.h"
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Engine.h"
#include "Goknar/GoknarAssert.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Materials/MaterialBase.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/IMeshInstance.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/WindowManager.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"

#include "Objects/FreeCameraObject.h"

FreeCameraController::FreeCameraController(FreeCameraObject* freeCameraObject) :
	freeCameraObject_(freeCameraObject),
	isRotatingTheCamera_(false),
	isMovingCameraIn2D_(false)
{
	movementSpeed_ = 50.f;
	previousCursorPositionForRotating_ = Vector2(0.f, 0.f);
	previousCursorPositionFor2DMovement_ = Vector2(0.f, 0.f);

	onMouseRightClickPressedDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::OnMouseRightClickPressed>(this);
	onMouseRightClickReleasedDelegate_ = KeyboardDelegate::create < FreeCameraController, &FreeCameraController::OnMouseRightClickReleased > (this);
	onMouseMiddleClickPressedDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::OnMouseMiddleClickPressed>(this);
	onMouseMiddleClickReleasedDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::OnMouseMiddleClickReleased>(this);

	moveLeftDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveLeftListener>(this);
	moveRightDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveRightListener>(this);
	moveForwardDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveForwardListener>(this);
	moveBackwardDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveBackwardListener>(this);
	moveUpDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveUpListener>(this);
	moveDownDelegate_ = KeyboardDelegate::create<FreeCameraController, &FreeCameraController::MoveDownListener>(this);

	onScrollMoveDelegate_ = Delegate<void(double, double)>::create<FreeCameraController, &FreeCameraController::ScrollListener>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::create<FreeCameraController, &FreeCameraController::CursorMovement>(this);
}

FreeCameraController::~FreeCameraController()
{
	UnbindInputDelegates();
}

void FreeCameraController::BeginGame()
{
	if (GetIsActive())
	{
		BindInputDelegates();
	}
}

void FreeCameraController::SetupInputs()
{
}

void FreeCameraController::SetIsActive(bool isActive)
{
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

void FreeCameraController::CursorMovement(double x, double y)
{
	Vector2 currentCursorPosition(x, y);

	if (isRotatingTheCamera_)
	{
		Vector2 cursorMovementVector = (previousCursorPositionForRotating_ - currentCursorPosition) / 250.f;
		Yaw(cursorMovementVector.x);
		Pitch(-cursorMovementVector.y);

		previousCursorPositionForRotating_ = currentCursorPosition;
	}

	if (isMovingCameraIn2D_)
	{
		Vector2 cursorMovementVector = currentCursorPosition - previousCursorPositionFor2DMovement_;

		MoveLeft(-cursorMovementVector.x);
		MoveUp(-cursorMovementVector.y);

		previousCursorPositionFor2DMovement_ = currentCursorPosition;
	}
}

void FreeCameraController::ScrollListener(double x, double y)
{
	MoveForward(y * movementSpeed_);
}

void FreeCameraController::Yaw(float value)
{
	Vector3 newForwardVector = freeCameraObject_->GetForwardVector().RotateVector(Vector3::UpVector * value);
	freeCameraObject_->SetWorldRotation(Quaternion::FromTwoVectors(Vector3::ForwardVector, newForwardVector));
}

void FreeCameraController::Pitch(float value)
{
	Vector3 newForwardVector = freeCameraObject_->GetForwardVector().RotateVector(freeCameraObject_->GetLeftVector() * value);
	freeCameraObject_->SetWorldRotation(Quaternion::FromTwoVectors(Vector3::ForwardVector, newForwardVector));
}

void FreeCameraController::OnMouseRightClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetWindow(), x, y);
	previousCursorPositionForRotating_ = Vector2(x, y);
	isRotatingTheCamera_ = true;
}

void FreeCameraController::OnMouseMiddleClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetWindow(), x, y);
	previousCursorPositionFor2DMovement_ = Vector2(x, y);
	isMovingCameraIn2D_ = true;
}

void FreeCameraController::MoveForward(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetForwardVector() * engine->GetDeltaTime() * multiplier);
}

void FreeCameraController::MoveLeft(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetLeftVector() * engine->GetDeltaTime() * multiplier);
}

void FreeCameraController::MoveUp(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetUpVector() * engine->GetDeltaTime() * multiplier);
}

void FreeCameraController::BindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);

	inputManager->AddKeyboardInputDelegate(KEY_MAP::LEFT, INPUT_ACTION::G_REPEAT, moveLeftDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::RIGHT, INPUT_ACTION::G_REPEAT, moveRightDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::UP, INPUT_ACTION::G_REPEAT, moveForwardDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::DOWN, INPUT_ACTION::G_REPEAT, moveBackwardDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::SPACE, INPUT_ACTION::G_REPEAT, moveUpDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::LEFT_CONTROL, INPUT_ACTION::G_REPEAT, moveDownDelegate_);

	inputManager->AddCursorDelegate(onCursorMoveDelegate_);
	inputManager->AddScrollDelegate(onScrollMoveDelegate_);

}

void FreeCameraController::UnbindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);

	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::LEFT, INPUT_ACTION::G_REPEAT, moveLeftDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::RIGHT, INPUT_ACTION::G_REPEAT, moveRightDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::UP, INPUT_ACTION::G_REPEAT, moveForwardDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::DOWN, INPUT_ACTION::G_REPEAT, moveBackwardDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::SPACE, INPUT_ACTION::G_REPEAT, moveUpDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::LEFT_CONTROL, INPUT_ACTION::G_REPEAT, moveDownDelegate_);

	inputManager->RemoveCursorDelegate(onScrollMoveDelegate_);
	inputManager->RemoveScrollDelegate(onCursorMoveDelegate_);
}
