#pragma once

#include "EditorPanel.h"
#include "DebugPanel.h"
#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Math/Matrix.h"
#include "Goknar/Math/Quaternion.h"

#include <unordered_map>
#include <vector>

class Camera;
class MeshUnit;
class ObjectBase;
class StaticMesh;

enum class EditorTransformGizmoAxis
{
	None = -1,
	X = 0,
	Y,
	Z,
	Center
};

enum class EditorTransformGizmoMode
{
	Translate,
	Rotate,
	Scale
};

struct EditorCollisionGameObjectPair
{
	ObjectBase* gameObject{ nullptr };
	const MeshUnit* meshUnit{ nullptr };
	Matrix transformationMatrix{ Matrix::IdentityMatrix };
};

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorHUD* hud);
	virtual ~ViewportPanel();

	virtual void Init() override;
	virtual void Draw() override;

	bool GetDebugOverlayEnabled() const
	{
		return showDebugOverlay_;
	}

	void SetDebugOverlayEnabled(bool enabled) 
	{
		showDebugOverlay_ = enabled;
	}

	bool HandleViewportLeftClick();
	bool HandleViewportRightClick();

private:
	void UpdateCollisionGameObjectPairs();
	bool IsCursorInsideViewport() const;
	bool GetViewportCursorPosition(Vector2& outPosition) const;
	bool GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const;
	ObjectBase* GetGameObjectUnderCursor();
	const EditorCollisionGameObjectPair* GetSelectedCollisionGameObjectPair(ObjectBase* selectedObject) const;
	bool RayIntersectsMeshCollider(
		const Vector3& rayOrigin,
		const Vector3& rayDirection,
		const EditorCollisionGameObjectPair& collisionGameObjectPair,
		float& outDistance) const;
	bool RayIntersectsTriangle(
		const Vector3& rayOrigin,
		const Vector3& rayDirection,
		const Vector3& vertex0,
		const Vector3& vertex1,
		const Vector3& vertex2,
		float& outDistance) const;
	void LoadTransformGizmoMesh();
	void UpdateTransformGizmo();
	void UpdateTransformGizmoDrag();
	void EndTransformGizmoDrag(bool markDirty);
	bool TryHandleTransformGizmoClick();
	bool FindTransformGizmoAxisUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, EditorTransformGizmoAxis& outAxis) const;
	bool GetTransformGizmoRotationDirectionUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, EditorTransformGizmoAxis axis, Vector3& outDirection) const;
	Vector3 GetTransformGizmoOrigin() const;
	float GetTransformGizmoScale(const Vector3& origin) const;
	Vector3 GetTransformGizmoAxisVector(EditorTransformGizmoAxis axis) const;
	void SetTransformGizmoMode(EditorTransformGizmoMode mode);
	void UpdateTransformGizmoModeShortcuts();
	void DrawTransformGizmoModeToolbar();
	void DrawTransformGizmo(Camera* camera) const;
	void DrawSelectedObjectHighlight() const;
	void DrawMeshColliderHighlight(Camera* camera, const EditorCollisionGameObjectPair& collisionGameObjectPair) const;

	DebugPanel* debugPanel_{ nullptr };
	StaticMesh* transformGizmoMesh_{ nullptr };
	EditorTransformGizmoMode transformGizmoMode_{ EditorTransformGizmoMode::Translate };
	EditorTransformGizmoAxis selectedTransformGizmoAxis_{ EditorTransformGizmoAxis::None };
	bool isTransformGizmoModeToolbarHovered_{ false };
	bool isDraggingTransformGizmo_{ false };
	Vector3 dragStartGizmoOrigin_{ Vector3::ZeroVector };
	float dragStartAxisParameter_{ 0.f };
	Vector3 dragStartRotationDirection_{ Vector3::ZeroVector };
	Vector2 dragStartCursorPosition_{ Vector2::ZeroVector };
	float transformGizmoScale_{ 1.f };
	std::vector<ObjectBase*> draggedTransformGizmoObjects_;
	std::vector<Vector3> dragStartObjectPositions_;
	std::vector<Quaternion> dragStartObjectRotations_;
	std::vector<Vector3> dragStartObjectScalings_;
	std::vector<EditorCollisionGameObjectPair> collisionGameObjectPairs_;
	std::unordered_map<ObjectBase*, EditorCollisionGameObjectPair> selectedCollisionGameObjectPairsByObject_;
	EditorCollisionGameObjectPair selectedCollisionGameObjectPair_{};
	bool hasSelectedCollisionGameObjectPair_{ false };
	bool showDebugOverlay_{ true };
};
