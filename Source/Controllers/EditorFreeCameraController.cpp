#include "pch.h"

#include "EditorFreeCameraController.h"

#include "Goknar/Scene.h"

#include "Goknar/Application.h"
#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/GoknarAssert.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/WindowManager.h"
#include "Goknar/Materials/MaterialBase.h"
#include "Goknar/Model/IMeshInstance.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"

#include "Objects/EditorFreeCameraObject.h"

EditorFreeCameraController::EditorFreeCameraController(EditorFreeCameraObject* freeCameraObject) :
	freeCameraObject_(freeCameraObject),
	isRotatingTheCamera_(false),
	isMovingCameraIn2D_(false)
{
	movementSpeed_ = 50.f;
	previousCursorPositionForRotating_ = Vector2(0.f, 0.f);
	previousCursorPositionFor2DMovement_ = Vector2(0.f, 0.f);

	onMouseRightClickPressedDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::OnMouseRightClickPressed>(this);
	onMouseRightClickReleasedDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::OnMouseRightClickReleased >(this);
	onMouseMiddleClickPressedDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::OnMouseMiddleClickPressed>(this);
	onMouseMiddleClickReleasedDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::OnMouseMiddleClickReleased>(this);

	moveUpDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::MoveUpListener>(this);
	moveDownDelegate_ = KeyboardDelegate::Create<EditorFreeCameraController, &EditorFreeCameraController::MoveDownListener>(this);

	keyboardInputDelegate_ = Delegate<void(int, int, int, int)>::Create<EditorFreeCameraController, &EditorFreeCameraController::KeyboardInputListener>(this);
	onScrollMoveDelegate_ = Delegate<void(double, double)>::Create<EditorFreeCameraController, &EditorFreeCameraController::ScrollListener>(this);
	onCursorMoveDelegate_ = Delegate<void(double, double)>::Create<EditorFreeCameraController, &EditorFreeCameraController::CursorMovement>(this);

	SetIsTickable(true);
}

EditorFreeCameraController::~EditorFreeCameraController()
{
	UnbindInputDelegates();
}

void EditorFreeCameraController::BeginGame()
{
	if (GetIsActive())
	{
		BindInputDelegates();
	}
}

void EditorFreeCameraController::SetupInputDelegates()
{
}

void EditorFreeCameraController::Tick(float deltaTime)
{
	if (!GetIsActive())
	{
		return;
	}

	const float movementMultiplier = movementSpeed_ * 20.f * deltaTime;
	if (isMoveForwardPressed_)
	{
		MoveForward(movementMultiplier);
	}

	if (isMoveBackwardPressed_)
	{
		MoveForward(-movementMultiplier);
	}

	if (isMoveLeftPressed_)
	{
		MoveLeft(movementMultiplier);
	}

	if (isMoveRightPressed_)
	{
		MoveLeft(-movementMultiplier);
	}
}

void EditorFreeCameraController::SetIsActive(bool isActive)
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

void EditorFreeCameraController::KeyboardInputListener(int key, int, int action, int)
{
	const bool isPressed = action != GLFW_RELEASE;
	if (key == static_cast<int>(KEY_MAP::UP))
	{
		isMoveForwardPressed_ = isPressed;
	}
	else if (key == static_cast<int>(KEY_MAP::DOWN))
	{
		isMoveBackwardPressed_ = isPressed;
	}
	else if (key == static_cast<int>(KEY_MAP::LEFT))
	{
		isMoveLeftPressed_ = isPressed;
	}
	else if (key == static_cast<int>(KEY_MAP::RIGHT))
	{
		isMoveRightPressed_ = isPressed;
	}
}

void EditorFreeCameraController::CursorMovement(double x, double y)
{
	Vector2 currentCursorPosition((float)x, (float)y);

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

void EditorFreeCameraController::ScrollListener(double, double y)
{
	MoveForward((float)y * movementSpeed_);
}

void EditorFreeCameraController::Yaw(float value)
{
	Vector3 newForwardVector = freeCameraObject_->GetWorldRotation().ToEulerRadians();
	freeCameraObject_->SetWorldRotation(Quaternion::FromEulerRadians(newForwardVector + Vector3{ 0.f, 0.f, value }));
}

void EditorFreeCameraController::Pitch(float value)
{
	Vector3 newForwardVector = freeCameraObject_->GetForwardVector().RotateVectorAroundAxis(freeCameraObject_->GetLeftVector(), value);
	freeCameraObject_->SetWorldRotation(newForwardVector.GetRotationNormalized());
}

void EditorFreeCameraController::OnMouseRightClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetMainWindow(), x, y);
	previousCursorPositionForRotating_ = Vector2((float)x, (float)y);
	isRotatingTheCamera_ = true;
}

void EditorFreeCameraController::OnMouseMiddleClickPressed()
{
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetMainWindow(), x, y);
	previousCursorPositionFor2DMovement_ = Vector2((float)x, (float)y);
	isMovingCameraIn2D_ = true;
}

void EditorFreeCameraController::MoveForward(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetForwardVector() * 0.025f * multiplier);
}

void EditorFreeCameraController::MoveLeft(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetLeftVector() * 0.025f * multiplier);
}

void EditorFreeCameraController::MoveUp(float multiplier/* = 1.f*/)
{
	freeCameraObject_->SetWorldPosition(
		freeCameraObject_->GetWorldPosition() +
		freeCameraObject_->GetUpVector() * 0.025f * multiplier);
}

void EditorFreeCameraController::ResetMovementInput()
{
	isMoveForwardPressed_ = false;
	isMoveBackwardPressed_ = false;
	isMoveLeftPressed_ = false;
	isMoveRightPressed_ = false;
}

void EditorFreeCameraController::BindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->AddMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);

	inputManager->AddKeyboardListener(keyboardInputDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::SPACE, INPUT_ACTION::G_REPEAT, moveUpDelegate_);
	inputManager->AddKeyboardInputDelegate(KEY_MAP::LEFT_CONTROL, INPUT_ACTION::G_REPEAT, moveDownDelegate_);

	inputManager->AddScrollDelegate(onScrollMoveDelegate_);
	inputManager->AddCursorDelegate(onCursorMoveDelegate_);

}

void EditorFreeCameraController::UnbindInputDelegates()
{
	InputManager* inputManager = engine->GetInputManager();

	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_PRESS, onMouseRightClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_RIGHT, INPUT_ACTION::G_RELEASE, onMouseRightClickReleasedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_PRESS, onMouseMiddleClickPressedDelegate_);
	inputManager->RemoveMouseInputDelegate(MOUSE_MAP::BUTTON_MIDDLE, INPUT_ACTION::G_RELEASE, onMouseMiddleClickReleasedDelegate_);

	// Trigger release events to cancel any ongoing operation
	onMouseRightClickReleasedDelegate_();
	onMouseMiddleClickReleasedDelegate_();

	inputManager->RemoveKeyboardListener(keyboardInputDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::SPACE, INPUT_ACTION::G_REPEAT, moveUpDelegate_);
	inputManager->RemoveKeyboardInputDelegate(KEY_MAP::LEFT_CONTROL, INPUT_ACTION::G_REPEAT, moveDownDelegate_);
	ResetMovementInput();

	inputManager->RemoveScrollDelegate(onScrollMoveDelegate_);
	inputManager->RemoveCursorDelegate(onCursorMoveDelegate_);
}
