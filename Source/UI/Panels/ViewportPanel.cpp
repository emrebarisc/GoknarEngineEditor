#include "ViewportPanel.h"

#include <cmath>
#include <limits>

#include "Goknar/Application.h"
#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/DynamicMeshComponent.h"
#include "Goknar/Components/InstancedStaticMeshComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Engine.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Model/InstancedStaticMesh.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Scene.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorUtils.h"

#include "Controllers/EditorFreeCameraController.h"
#include "Objects/EditorFreeCameraObject.h"

namespace
{
	constexpr ImU32 SelectedObjectShadowColor = IM_COL32(0, 0, 0, 220);
	constexpr ImU32 SelectedObjectHighlightColor = IM_COL32(255, 210, 55, 255);
	constexpr size_t MaxMeshHighlightEdges = 6000;
	constexpr float TransformGizmoAxisLength = 1.75f;
	constexpr float TransformGizmoAxisPickRadius = 0.16f;
	constexpr float TransformGizmoCameraDistanceScale = 0.075f;
	constexpr float TransformGizmoMinScale = 0.25f;
	constexpr float TransformGizmoMaxScale = 30.f;
	constexpr float TransformGizmoRotationRadius = 1.25f;
	constexpr float TransformGizmoRotationPickRadius = 0.12f;
	constexpr float TransformGizmoScaleHandleSize = 5.f;
	constexpr float TransformGizmoCenterHandleSize = 6.f;
	constexpr float TransformGizmoCenterPickRadius = 0.22f;
	constexpr float TransformGizmoScaleSensitivity = 0.75f;
	constexpr float TransformGizmoUniformScaleScreenSensitivity = 0.01f;
	constexpr float TransformGizmoMinimumObjectScale = 0.001f;
	constexpr int TransformGizmoRingSegmentCount = 72;

	bool ShouldSkipObject(const ObjectBase* object)
	{
		return !object ||
			!object->GetIsActive() ||
			object->GetName().find("__Editor__") != std::string::npos;
	}

	bool HasMeshColliderData(const MeshUnit* meshUnit)
	{
		const VertexArray* vertices = meshUnit ? meshUnit->GetVerticesPointer() : nullptr;
		const FaceArray* faces = meshUnit ? meshUnit->GetFacesPointer() : nullptr;
		return vertices && faces && !vertices->empty() && !faces->empty();
	}

	unsigned GetTransformGizmoAxisIndex(EditorTransformGizmoAxis axis)
	{
		switch (axis)
		{
		case EditorTransformGizmoAxis::X:
			return 0;
		case EditorTransformGizmoAxis::Y:
			return 1;
		case EditorTransformGizmoAxis::Z:
			return 2;
		default:
			return 0;
		}
	}

	ImU32 GetTransformGizmoAxisColor(EditorTransformGizmoAxis axis, bool isActive = false)
	{
		if (isActive)
		{
			return IM_COL32(255, 255, 255, 255);
		}

		switch (axis)
		{
		case EditorTransformGizmoAxis::X:
			return IM_COL32(255, 45, 45, 255);
		case EditorTransformGizmoAxis::Y:
			return IM_COL32(60, 230, 60, 255);
		case EditorTransformGizmoAxis::Z:
			return IM_COL32(70, 130, 255, 255);
		default:
			return IM_COL32(255, 255, 255, 255);
		}
	}

	bool GetTransformGizmoRingBasis(const Vector3& axisDirection, Vector3& outTangent, Vector3& outBitangent)
	{
		if (axisDirection.SquareLength() <= SMALLER_EPSILON)
		{
			return false;
		}

		outTangent = axisDirection.GetOrthonormalBasis();
		outBitangent = axisDirection.Cross(outTangent).GetNormalized();
		return outTangent.SquareLength() > SMALLER_EPSILON &&
			outBitangent.SquareLength() > SMALLER_EPSILON;
	}

	template<typename MeshComponentType>
	void AddMeshComponentCollisionGameObjectPairs(
		ObjectBase* object,
		Camera* camera,
		std::vector<EditorCollisionGameObjectPair>& collisionGameObjectPairs)
	{
		std::vector<MeshComponentType*> meshComponents = object->GetComponentsOfType<MeshComponentType>();
		for (MeshComponentType* meshComponent : meshComponents)
		{
			if (!meshComponent || !meshComponent->GetIsActive())
			{
				continue;
			}

			auto* meshInstance = meshComponent->GetMeshInstance();
			if (!meshInstance ||
				!meshInstance->GetIsRendered() ||
				!(camera->GetRenderMask() & meshInstance->GetRenderMask()))
			{
				continue;
			}

			auto* mesh = meshInstance->GetMesh();
			if (!mesh)
			{
				continue;
			}

			const Matrix& transformationMatrix = meshComponent->GetComponentToWorldTransformationMatrix();
			const auto& subMeshes = mesh->GetSubMeshes();
			for (const MeshUnit* subMesh : subMeshes)
			{
				if (!HasMeshColliderData(subMesh))
				{
					continue;
				}

				if (!camera->IsAABBVisible(subMesh->GetAABB(), transformationMatrix))
				{
					continue;
				}

				collisionGameObjectPairs.push_back({ object, subMesh, transformationMatrix });
			}
		}
	}

	void AddInstancedStaticMeshCollisionGameObjectPairs(
		ObjectBase* object,
		Camera* camera,
		std::vector<EditorCollisionGameObjectPair>& collisionGameObjectPairs)
	{
		std::vector<InstancedStaticMeshComponent*> meshComponents = object->GetComponentsOfType<InstancedStaticMeshComponent>();
		for (InstancedStaticMeshComponent* meshComponent : meshComponents)
		{
			if (!meshComponent || !meshComponent->GetIsActive())
			{
				continue;
			}

			auto* meshInstance = meshComponent->GetMeshInstance();
			if (!meshInstance ||
				!meshInstance->GetIsRendered() ||
				!(camera->GetRenderMask() & meshInstance->GetRenderMask()))
			{
				continue;
			}

			InstancedStaticMesh* mesh = meshInstance->GetMesh();
			if (!mesh)
			{
				continue;
			}

			const Matrix& componentTransformationMatrix = meshComponent->GetComponentToWorldTransformationMatrix();
			for (size_t instanceIndex = 0; instanceIndex < mesh->GetInstanceCount(); ++instanceIndex)
			{
				const Matrix transformationMatrix = mesh->GetInstanceTransformationAt(instanceIndex) * componentTransformationMatrix;
				const auto& subMeshes = mesh->GetSubMeshes();
				for (const MeshUnit* subMesh : subMeshes)
				{
					if (!HasMeshColliderData(subMesh))
					{
						continue;
					}

					if (!camera->IsAABBVisible(subMesh->GetAABB(), transformationMatrix))
					{
						continue;
					}

					collisionGameObjectPairs.push_back({ object, subMesh, transformationMatrix });
				}
			}
		}
	}

	Vector3 TransformPoint(const Matrix& transformationMatrix, const Vector3& point)
	{
		return Vector3(transformationMatrix * Vector4(point, 1.f));
	}

	float RaySegmentDistanceSquared(const Vector3& rayOrigin, const Vector3& rayDirection, const Vector3& segmentStart, const Vector3& segmentEnd)
	{
		const Vector3 segment = segmentEnd - segmentStart;
		const float segmentLengthSquared = segment.SquareLength();
		if (segmentLengthSquared <= SMALLER_EPSILON)
		{
			const Vector3 pointDelta = segmentStart - rayOrigin;
			const float rayParameter = GoknarMath::Max(pointDelta.Dot(rayDirection), 0.f);
			const Vector3 closestOnRay = rayOrigin + rayDirection * rayParameter;
			return Vector3::SquareDistance(segmentStart, closestOnRay);
		}

		const Vector3 rayToSegmentStart = rayOrigin - segmentStart;
		const float raySegmentDot = rayDirection.Dot(segment);
		const float rayStartDot = rayDirection.Dot(rayToSegmentStart);
		const float segmentStartDot = segment.Dot(rayToSegmentStart);
		const float denominator = segmentLengthSquared - raySegmentDot * raySegmentDot;

		float rayParameter = 0.f;
		float segmentParameter = 0.f;

		if (denominator > SMALLER_EPSILON)
		{
			rayParameter = (raySegmentDot * segmentStartDot - segmentLengthSquared * rayStartDot) / denominator;
			segmentParameter = (segmentStartDot - raySegmentDot * rayStartDot) / denominator;
		}
		else
		{
			segmentParameter = GoknarMath::Clamp(segmentStartDot / segmentLengthSquared, 0.f, 1.f);
		}

		if (rayParameter < 0.f)
		{
			rayParameter = 0.f;
			segmentParameter = GoknarMath::Clamp(segmentStartDot / segmentLengthSquared, 0.f, 1.f);
		}
		else if (segmentParameter < 0.f)
		{
			segmentParameter = 0.f;
			rayParameter = GoknarMath::Max(-rayStartDot, 0.f);
		}
		else if (1.f < segmentParameter)
		{
			segmentParameter = 1.f;
			rayParameter = GoknarMath::Max(raySegmentDot - rayStartDot, 0.f);
		}

		const Vector3 closestOnRay = rayOrigin + rayDirection * rayParameter;
		const Vector3 closestOnSegment = segmentStart + segment * segmentParameter;
		return Vector3::SquareDistance(closestOnRay, closestOnSegment);
	}

	bool GetAxisParameterFromRay(const Vector3& rayOrigin, const Vector3& rayDirection, const Vector3& axisOrigin, const Vector3& axisDirection, float& outParameter)
	{
		const float rayAxisDot = rayDirection.Dot(axisDirection);
		const float denominator = 1.f - rayAxisDot * rayAxisDot;
		if (denominator <= SMALLER_EPSILON)
		{
			return false;
		}

		const Vector3 axisToRay = axisOrigin - rayOrigin;
		outParameter = (rayAxisDot * rayDirection.Dot(axisToRay) - axisDirection.Dot(axisToRay)) / denominator;
		return true;
	}

	float GetRotationRingDistanceSquared(
		const Vector3& rayOrigin,
		const Vector3& rayDirection,
		const Vector3& ringOrigin,
		const Vector3& axisDirection,
		float ringRadius)
	{
		Vector3 tangent;
		Vector3 bitangent;
		if (!GetTransformGizmoRingBasis(axisDirection, tangent, bitangent))
		{
			return MAX_FLOAT;
		}

		float closestDistanceSquared = MAX_FLOAT;
		Vector3 previousRingPoint = ringOrigin + tangent * ringRadius;
		for (int segmentIndex = 1; segmentIndex <= TransformGizmoRingSegmentCount; ++segmentIndex)
		{
			const float angle = (TWO_PI * static_cast<float>(segmentIndex)) / static_cast<float>(TransformGizmoRingSegmentCount);
			const Vector3 currentRingPoint = ringOrigin +
				(tangent * std::cos(angle) + bitangent * std::sin(angle)) * ringRadius;
			closestDistanceSquared = GoknarMath::Min(
				closestDistanceSquared,
				RaySegmentDistanceSquared(rayOrigin, rayDirection, previousRingPoint, currentRingPoint));
			previousRingPoint = currentRingPoint;
		}

		return closestDistanceSquared;
	}

	float GetSignedAngleAroundAxis(const Vector3& startDirection, const Vector3& currentDirection, const Vector3& axisDirection)
	{
		const float sinAngle = axisDirection.Dot(startDirection.Cross(currentDirection));
		const float cosAngle = GoknarMath::Clamp(startDirection.Dot(currentDirection), -1.f, 1.f);
		return std::atan2(sinAngle, cosAngle);
	}
}

ViewportPanel::ViewportPanel(EditorHUD* hud) : 
	IEditorPanel("Viewport", hud)
{
	size_ = Vector2(1024, 768);
}

ViewportPanel::~ViewportPanel()
{
}

void ViewportPanel::Init()
{
	Camera* renderTargetCamera = EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera();
	EditorContext::Get()->viewportRenderTarget->SetCamera(renderTargetCamera);

    debugPanel_ = static_cast<DebugPanel*>(hud_->GetPanel<DebugPanel>());
}

void ViewportPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	isTransformGizmoModeToolbarHovered_ = false;

	bool isHovered = ImGui::IsItemHovered() || ImGui::IsWindowHovered();
	isHovered |= debugPanel_->IsItemHovered();

	EditorContext::Get()->viewportCameraObject->GetController()->SetIsActive(isHovered);

	ImVec2 newViewportSize = ImGui::GetContentRegionAvail();

	if (newViewportSize.x <= 0.0f || newViewportSize.y <= 0.0f)
	{
		ImGui::End();
		return;
	}

	if (size_.x != newViewportSize.x || size_.y != newViewportSize.y)
	{
		size_ = EditorUtils::ToVector2(newViewportSize);
		EditorContext::Get()->viewportRenderTarget->SetFrameSize({ newViewportSize.x, newViewportSize.y });
	}

	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	position_ = Vector2(cursorScreenPos.x, cursorScreenPos.y);
	UpdateTransformGizmoModeShortcuts();
	LoadTransformGizmoMesh();
	UpdateTransformGizmoDrag();
	UpdateTransformGizmo();
	UpdateCollisionGameObjectPairs();

	Texture* renderTargetTexture = EditorContext::Get()->viewportRenderTarget->GetTexture();

	ImGui::Image(
		(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
		EditorUtils::ToImVec2(size_),
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PAYLOAD"))
		{
			const std::string sourcePath = static_cast<const char*>(payload->Data);
			hud_->InsertSceneReference(sourcePath, hud_->RaycastWorld());
		}

		ImGui::EndDragDropTarget();
	}

	DrawTransformGizmoModeToolbar();
	DrawSelectedObjectHighlight();
	DrawTransformGizmo(EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera());
	EditorUtils::DrawWorldAxis(EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera());

	if (showDebugOverlay_ && debugPanel_)
	{
		debugPanel_->DrawOverlay(position_, size_);
	}

	ImGui::End();
}

bool ViewportPanel::HandleViewportLeftClick()
{
	if (debugPanel_ && debugPanel_->IsItemHovered())
	{
		return false;
	}

	if (!IsCursorInsideViewport())
	{
		return false;
	}

	if (isTransformGizmoModeToolbarHovered_)
	{
		return false;
	}

	if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl && TryHandleTransformGizmoClick())
	{
		return true;
	}

	ObjectBase* selectedObject = GetGameObjectUnderCursor();
	EditorContext* context = EditorContext::Get();
	if (selectedObject)
	{
		const bool shouldAddSelection = ImGui::GetIO().KeyShift;
		const bool shouldRemoveSelection = ImGui::GetIO().KeyCtrl;

		if (shouldRemoveSelection)
		{
			context->RemoveObjectSelection(selectedObject);
			selectedCollisionGameObjectPairsByObject_.erase(selectedObject);
			return true;
		}

		if (!shouldAddSelection)
		{
			selectedCollisionGameObjectPairsByObject_.clear();
		}

		context->SetObjectSelection(selectedObject, shouldAddSelection);
		if (hasSelectedCollisionGameObjectPair_)
		{
			selectedCollisionGameObjectPairsByObject_[selectedObject] = selectedCollisionGameObjectPair_;
		}
	}
	else if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
	{
		context->ClearSelection();
		selectedCollisionGameObjectPairsByObject_.clear();
	}

	return true;
}

bool ViewportPanel::HandleViewportRightClick()
{
	if (debugPanel_ && debugPanel_->IsItemHovered())
	{
		return false;
	}

	if (!IsCursorInsideViewport())
	{
		return false;
	}

	if (isTransformGizmoModeToolbarHovered_)
	{
		return false;
	}

	ObjectBase* selectedObject = GetGameObjectUnderCursor();
	EditorContext* context = EditorContext::Get();
	if (selectedObject && context->IsObjectSelected(selectedObject))
	{
		context->RemoveObjectSelection(selectedObject);
		selectedCollisionGameObjectPairsByObject_.erase(selectedObject);
	}

	return true;
}

void ViewportPanel::UpdateCollisionGameObjectPairs()
{
	collisionGameObjectPairs_.clear();

	EditorContext* context = EditorContext::Get();
	if (!context || !context->viewportRenderTarget || !engine || !engine->GetApplication())
	{
		return;
	}

	Camera* camera = context->viewportRenderTarget->GetCamera();
	Scene* currentScene = engine->GetApplication()->GetMainScene();
	if (!camera || !currentScene)
	{
		return;
	}

	const std::vector<ObjectBase*>& objects = currentScene->GetObjects();
	for (ObjectBase* object : objects)
	{
		if (ShouldSkipObject(object))
		{
			continue;
		}

		AddMeshComponentCollisionGameObjectPairs<StaticMeshComponent>(object, camera, collisionGameObjectPairs_);
		AddMeshComponentCollisionGameObjectPairs<SkeletalMeshComponent>(object, camera, collisionGameObjectPairs_);
		AddMeshComponentCollisionGameObjectPairs<DynamicMeshComponent>(object, camera, collisionGameObjectPairs_);
		AddInstancedStaticMeshCollisionGameObjectPairs(object, camera, collisionGameObjectPairs_);
	}
}

bool ViewportPanel::IsCursorInsideViewport() const
{
	if (size_.x <= 0 || size_.y <= 0 || !engine)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	return position_.x <= xPosition &&
		xPosition <= position_.x + size_.x &&
		position_.y <= yPosition &&
		yPosition <= position_.y + size_.y;
}

bool ViewportPanel::GetViewportCursorPosition(Vector2& outPosition) const
{
	if (!engine)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);
	outPosition = Vector2((float)xPosition, (float)yPosition);
	return true;
}

bool ViewportPanel::GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const
{
	EditorContext* context = EditorContext::Get();
	if (!context || !context->viewportRenderTarget || !engine)
	{
		return false;
	}

	Camera* activeCamera = context->viewportRenderTarget->GetCamera();
	if (!activeCamera)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	outOrigin = activeCamera->GetPosition();
	outDirection = activeCamera->GetWorldDirectionAtPixel(
		Vector2i{ (int)(xPosition - position_.x), (int)(yPosition - position_.y) }).GetNormalized();

	return outDirection.SquareLength() > SMALLER_EPSILON;
}

ObjectBase* ViewportPanel::GetGameObjectUnderCursor()
{
	UpdateCollisionGameObjectPairs();

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return nullptr;
	}

	ObjectBase* selectedObject = nullptr;
	float closestDistance = std::numeric_limits<float>::max();
	hasSelectedCollisionGameObjectPair_ = false;
	for (const EditorCollisionGameObjectPair& collisionGameObjectPair : collisionGameObjectPairs_)
	{
		float hitDistance = 0.f;
		if (!RayIntersectsMeshCollider(rayOrigin, rayDirection, collisionGameObjectPair, hitDistance))
		{
			continue;
		}

		if (hitDistance < closestDistance)
		{
			closestDistance = hitDistance;
			selectedObject = collisionGameObjectPair.gameObject;
			selectedCollisionGameObjectPair_ = collisionGameObjectPair;
			hasSelectedCollisionGameObjectPair_ = true;
		}
	}

	return selectedObject;
}

const EditorCollisionGameObjectPair* ViewportPanel::GetSelectedCollisionGameObjectPair(ObjectBase* selectedObject) const
{
	if (!selectedObject)
	{
		return nullptr;
	}

	const auto selectedCollisionGameObjectPairIterator = selectedCollisionGameObjectPairsByObject_.find(selectedObject);
	if (selectedCollisionGameObjectPairIterator != selectedCollisionGameObjectPairsByObject_.end() &&
		selectedCollisionGameObjectPairIterator->second.meshUnit)
	{
		for (const EditorCollisionGameObjectPair& collisionGameObjectPair : collisionGameObjectPairs_)
		{
			if (collisionGameObjectPair.gameObject == selectedObject &&
				collisionGameObjectPair.meshUnit == selectedCollisionGameObjectPairIterator->second.meshUnit)
			{
				return &collisionGameObjectPair;
			}
		}
	}

	if (hasSelectedCollisionGameObjectPair_ &&
		selectedCollisionGameObjectPair_.gameObject == selectedObject &&
		selectedCollisionGameObjectPair_.meshUnit)
	{
		return &selectedCollisionGameObjectPair_;
	}

	for (const EditorCollisionGameObjectPair& collisionGameObjectPair : collisionGameObjectPairs_)
	{
		if (collisionGameObjectPair.gameObject == selectedObject)
		{
			return &collisionGameObjectPair;
		}
	}

	return nullptr;
}

bool ViewportPanel::RayIntersectsMeshCollider(
	const Vector3& rayOrigin,
	const Vector3& rayDirection,
	const EditorCollisionGameObjectPair& collisionGameObjectPair,
	float& outDistance) const
{
	if (!collisionGameObjectPair.meshUnit)
	{
		return false;
	}

	const VertexArray* vertices = collisionGameObjectPair.meshUnit->GetVerticesPointer();
	const FaceArray* faces = collisionGameObjectPair.meshUnit->GetFacesPointer();
	if (!vertices || !faces || vertices->empty() || faces->empty())
	{
		return false;
	}

	const Matrix inverseTransformationMatrix = collisionGameObjectPair.transformationMatrix.GetInverse();
	const Vector3 localRayOrigin(inverseTransformationMatrix * Vector4(rayOrigin, 1.f));
	Vector3 localRayDirection(inverseTransformationMatrix * Vector4(rayDirection, 0.f));

	if (localRayOrigin.ContainsNanOrInf() ||
		localRayDirection.ContainsNanOrInf() ||
		localRayDirection.SquareLength() <= SMALLER_EPSILON)
	{
		return false;
	}

	localRayDirection = localRayDirection.GetNormalized();

	float tMin = 0.f;
	float tMax = std::numeric_limits<float>::max();
	const Vector3& minimum = collisionGameObjectPair.meshUnit->GetAABB().GetMin();
	const Vector3& maximum = collisionGameObjectPair.meshUnit->GetAABB().GetMax();

	for (int axis = 0; axis < 3; ++axis)
	{
		const float rayOriginOnAxis = localRayOrigin[axis];
		const float rayDirectionOnAxis = localRayDirection[axis];
		const float boxMinOnAxis = minimum[axis];
		const float boxMaxOnAxis = maximum[axis];

		if (GoknarMath::Abs(rayDirectionOnAxis) <= SMALLER_EPSILON)
		{
			if (rayOriginOnAxis < boxMinOnAxis || boxMaxOnAxis < rayOriginOnAxis)
			{
				return false;
			}

			continue;
		}

		const float inverseRayDirection = 1.f / rayDirectionOnAxis;
		float nearDistance = (boxMinOnAxis - rayOriginOnAxis) * inverseRayDirection;
		float farDistance = (boxMaxOnAxis - rayOriginOnAxis) * inverseRayDirection;

		if (farDistance < nearDistance)
		{
			const float temp = nearDistance;
			nearDistance = farDistance;
			farDistance = temp;
		}

		tMin = GoknarMath::Max(tMin, nearDistance);
		tMax = GoknarMath::Min(tMax, farDistance);
		if (tMax < tMin)
		{
			return false;
		}
	}

	float closestLocalDistance = std::numeric_limits<float>::max();
	for (const Face& face : *faces)
	{
		if (face.vertexIndices[0] >= vertices->size() ||
			face.vertexIndices[1] >= vertices->size() ||
			face.vertexIndices[2] >= vertices->size())
		{
			continue;
		}

		float triangleHitDistance = 0.f;
		if (!RayIntersectsTriangle(
			localRayOrigin,
			localRayDirection,
			vertices->at(face.vertexIndices[0]).position,
			vertices->at(face.vertexIndices[1]).position,
			vertices->at(face.vertexIndices[2]).position,
			triangleHitDistance))
		{
			continue;
		}

		if (triangleHitDistance < closestLocalDistance)
		{
			closestLocalDistance = triangleHitDistance;
		}
	}

	if (closestLocalDistance == std::numeric_limits<float>::max())
	{
		return false;
	}

	const Vector3 localHitPosition = localRayOrigin + localRayDirection * closestLocalDistance;
	const Vector3 worldHitPosition = TransformPoint(collisionGameObjectPair.transformationMatrix, localHitPosition);
	outDistance = Vector3::Distance(rayOrigin, worldHitPosition);
	return true;
}

bool ViewportPanel::RayIntersectsTriangle(
	const Vector3& rayOrigin,
	const Vector3& rayDirection,
	const Vector3& vertex0,
	const Vector3& vertex1,
	const Vector3& vertex2,
	float& outDistance) const
{
	const Vector3 edge1 = vertex1 - vertex0;
	const Vector3 edge2 = vertex2 - vertex0;
	const Vector3 pVector = rayDirection.Cross(edge2);
	const float determinant = edge1.Dot(pVector);

	if (GoknarMath::Abs(determinant) <= SMALLER_EPSILON)
	{
		return false;
	}

	const float inverseDeterminant = 1.f / determinant;
	const Vector3 tVector = rayOrigin - vertex0;
	const float u = tVector.Dot(pVector) * inverseDeterminant;
	if (u < 0.f || 1.f < u)
	{
		return false;
	}

	const Vector3 qVector = tVector.Cross(edge1);
	const float v = rayDirection.Dot(qVector) * inverseDeterminant;
	if (v < 0.f || 1.f < u + v)
	{
		return false;
	}

	const float t = edge2.Dot(qVector) * inverseDeterminant;
	if (t <= SMALLER_EPSILON)
	{
		return false;
	}

	outDistance = t;
	return true;
}

void ViewportPanel::LoadTransformGizmoMesh()
{
	if (transformGizmoMesh_)
	{
		return;
	}

	transformGizmoMesh_ = EditorUtils::GetEditorContent<StaticMesh>("Meshes/SM_Gizmo.fbx");
}

void ViewportPanel::UpdateTransformGizmo()
{
	EditorContext* context = EditorContext::Get();
	if (!context ||
		context->selectedObjectType != EditorSelectionType::Object ||
		context->GetSelectedObjects().empty())
	{
		EndTransformGizmoDrag(false);
		return;
	}

	const Vector3 gizmoOrigin = GetTransformGizmoOrigin();
	transformGizmoScale_ = GetTransformGizmoScale(gizmoOrigin);
}

void ViewportPanel::UpdateTransformGizmoDrag()
{
	if (!isDraggingTransformGizmo_)
	{
		return;
	}

	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		EndTransformGizmoDrag(true);
		return;
	}

	if (selectedTransformGizmoAxis_ == EditorTransformGizmoAxis::None ||
		draggedTransformGizmoObjects_.empty())
	{
		EndTransformGizmoDrag(false);
		return;
	}

	const bool hasMatchingStarts =
		draggedTransformGizmoObjects_.size() == dragStartObjectPositions_.size() &&
		draggedTransformGizmoObjects_.size() == dragStartObjectRotations_.size() &&
		draggedTransformGizmoObjects_.size() == dragStartObjectScalings_.size();
	if (!hasMatchingStarts)
	{
		EndTransformGizmoDrag(false);
		return;
	}

	if (transformGizmoMode_ == EditorTransformGizmoMode::Scale &&
		selectedTransformGizmoAxis_ == EditorTransformGizmoAxis::Center)
	{
		Vector2 currentCursorPosition;
		if (!GetViewportCursorPosition(currentCursorPosition))
		{
			return;
		}

		const Vector2 cursorDelta = currentCursorPosition - dragStartCursorPosition_;
		const float scaleDelta = (cursorDelta.x - cursorDelta.y) * TransformGizmoUniformScaleScreenSensitivity;
		const float positionScale = GoknarMath::Max(TransformGizmoMinimumObjectScale, 1.f + scaleDelta);
		const bool useCollectivePivot = ShouldUseCollectiveTransformPivot();
		for (size_t objectIndex = 0; objectIndex < draggedTransformGizmoObjects_.size(); ++objectIndex)
		{
			ObjectBase* draggedObject = draggedTransformGizmoObjects_[objectIndex];
			if (draggedObject)
			{
				Vector3 newScaling = dragStartObjectScalings_[objectIndex] + Vector3(scaleDelta);
				newScaling.x = GoknarMath::Max(TransformGizmoMinimumObjectScale, newScaling.x);
				newScaling.y = GoknarMath::Max(TransformGizmoMinimumObjectScale, newScaling.y);
				newScaling.z = GoknarMath::Max(TransformGizmoMinimumObjectScale, newScaling.z);
				if (useCollectivePivot)
				{
					const Vector3 objectOffset = dragStartObjectPositions_[objectIndex] - dragStartGizmoOrigin_;
					draggedObject->SetWorldPosition(dragStartGizmoOrigin_ + objectOffset * positionScale);
				}

				draggedObject->SetWorldScaling(newScaling);
			}
		}

		return;
	}

	const Vector3 axisDirection = GetTransformGizmoAxisVector(selectedTransformGizmoAxis_);
	if (axisDirection.SquareLength() <= SMALLER_EPSILON)
	{
		EndTransformGizmoDrag(false);
		return;
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return;
	}

	if (transformGizmoMode_ == EditorTransformGizmoMode::Rotate)
	{
		Vector3 currentRotationDirection = Vector3::ZeroVector;
		if (!GetTransformGizmoRotationDirectionUnderRay(rayOrigin, rayDirection, selectedTransformGizmoAxis_, currentRotationDirection))
		{
			return;
		}

		const float rotationDelta = GetSignedAngleAroundAxis(dragStartRotationDirection_, currentRotationDirection, axisDirection);
		const Quaternion rotationDeltaQuaternion = Quaternion::FromAxisAngle(axisDirection, rotationDelta);
		const bool useCollectivePivot = ShouldUseCollectiveTransformPivot();
		for (size_t objectIndex = 0; objectIndex < draggedTransformGizmoObjects_.size(); ++objectIndex)
		{
			ObjectBase* draggedObject = draggedTransformGizmoObjects_[objectIndex];
			if (draggedObject)
			{
				if (useCollectivePivot)
				{
					const Vector3 objectOffset = dragStartObjectPositions_[objectIndex] - dragStartGizmoOrigin_;
					draggedObject->SetWorldPosition(dragStartGizmoOrigin_ + rotationDeltaQuaternion * objectOffset);
				}

				draggedObject->SetWorldRotation((rotationDeltaQuaternion * dragStartObjectRotations_[objectIndex]).GetNormalized());
			}
		}

		return;
	}

	float currentAxisParameter = 0.f;
	if (!GetAxisParameterFromRay(rayOrigin, rayDirection, dragStartGizmoOrigin_, axisDirection, currentAxisParameter))
	{
		return;
	}

	if (transformGizmoMode_ == EditorTransformGizmoMode::Scale)
	{
		const unsigned axisIndex = GetTransformGizmoAxisIndex(selectedTransformGizmoAxis_);
		const float scaleDelta = ((currentAxisParameter - dragStartAxisParameter_) * TransformGizmoScaleSensitivity) /
			GoknarMath::Max(transformGizmoScale_, SMALLER_EPSILON);
		Vector3 positionScale(1.f);
		positionScale[axisIndex] = GoknarMath::Max(TransformGizmoMinimumObjectScale, 1.f + scaleDelta);
		const bool useCollectivePivot = ShouldUseCollectiveTransformPivot();
		for (size_t objectIndex = 0; objectIndex < draggedTransformGizmoObjects_.size(); ++objectIndex)
		{
			ObjectBase* draggedObject = draggedTransformGizmoObjects_[objectIndex];
			if (draggedObject)
			{
				Vector3 newScaling = dragStartObjectScalings_[objectIndex];
				newScaling[axisIndex] = GoknarMath::Max(
					TransformGizmoMinimumObjectScale,
					dragStartObjectScalings_[objectIndex][axisIndex] + scaleDelta);
				if (useCollectivePivot)
				{
					Vector3 objectOffset = dragStartObjectPositions_[objectIndex] - dragStartGizmoOrigin_;
					objectOffset *= positionScale;
					draggedObject->SetWorldPosition(dragStartGizmoOrigin_ + objectOffset);
				}

				draggedObject->SetWorldScaling(newScaling);
			}
		}

		return;
	}

	const Vector3 dragDelta = axisDirection * (currentAxisParameter - dragStartAxisParameter_);
	for (size_t objectIndex = 0; objectIndex < draggedTransformGizmoObjects_.size(); ++objectIndex)
	{
		ObjectBase* draggedObject = draggedTransformGizmoObjects_[objectIndex];
		if (draggedObject)
		{
			draggedObject->SetWorldPosition(dragStartObjectPositions_[objectIndex] + dragDelta);
		}
	}
}

void ViewportPanel::EndTransformGizmoDrag(bool markDirty)
{
	const bool wasDragging = isDraggingTransformGizmo_;
	isDraggingTransformGizmo_ = false;
	selectedTransformGizmoAxis_ = EditorTransformGizmoAxis::None;
	dragStartGizmoOrigin_ = Vector3::ZeroVector;
	dragStartAxisParameter_ = 0.f;
	dragStartRotationDirection_ = Vector3::ZeroVector;
	dragStartCursorPosition_ = Vector2::ZeroVector;
	draggedTransformGizmoObjects_.clear();
	dragStartObjectPositions_.clear();
	dragStartObjectRotations_.clear();
	dragStartObjectScalings_.clear();

	if (wasDragging && markDirty)
	{
		EditorContext::Get()->MarkSceneDirty("Object transform changed");
	}
}

bool ViewportPanel::TryHandleTransformGizmoClick()
{
	EditorContext* context = EditorContext::Get();
	if (!context ||
		context->selectedObjectType != EditorSelectionType::Object ||
		context->GetSelectedObjects().empty())
	{
		return false;
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return false;
	}

	EditorTransformGizmoAxis clickedAxis = EditorTransformGizmoAxis::None;
	if (!FindTransformGizmoAxisUnderRay(rayOrigin, rayDirection, clickedAxis))
	{
		return false;
	}

	Vector3 rotationDirection = Vector3::ZeroVector;
	if (transformGizmoMode_ == EditorTransformGizmoMode::Rotate &&
		!GetTransformGizmoRotationDirectionUnderRay(rayOrigin, rayDirection, clickedAxis, rotationDirection))
	{
		return false;
	}

	Vector2 cursorPosition;
	if (!GetViewportCursorPosition(cursorPosition))
	{
		return false;
	}

	selectedTransformGizmoAxis_ = clickedAxis;
	isDraggingTransformGizmo_ = true;
	dragStartGizmoOrigin_ = GetTransformGizmoOrigin();
	dragStartRotationDirection_ = rotationDirection;
	dragStartCursorPosition_ = cursorPosition;
	draggedTransformGizmoObjects_.clear();
	dragStartObjectPositions_.clear();
	dragStartObjectRotations_.clear();
	dragStartObjectScalings_.clear();

	for (ObjectBase* selectedObject : context->GetSelectedObjects())
	{
		if (selectedObject)
		{
			draggedTransformGizmoObjects_.push_back(selectedObject);
			dragStartObjectPositions_.push_back(selectedObject->GetWorldPosition());
			dragStartObjectRotations_.push_back(selectedObject->GetWorldRotation());
			dragStartObjectScalings_.push_back(selectedObject->GetWorldScaling());
		}
	}

	if (draggedTransformGizmoObjects_.empty())
	{
		EndTransformGizmoDrag(false);
		return false;
	}

	if (transformGizmoMode_ != EditorTransformGizmoMode::Rotate &&
		selectedTransformGizmoAxis_ != EditorTransformGizmoAxis::Center)
	{
		const Vector3 axisDirection = GetTransformGizmoAxisVector(selectedTransformGizmoAxis_);
		if (!GetAxisParameterFromRay(rayOrigin, rayDirection, dragStartGizmoOrigin_, axisDirection, dragStartAxisParameter_))
		{
			dragStartAxisParameter_ = 0.f;
		}
	}

	return true;
}

bool ViewportPanel::FindTransformGizmoAxisUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, EditorTransformGizmoAxis& outAxis) const
{
	outAxis = EditorTransformGizmoAxis::None;

	EditorContext* context = EditorContext::Get();
	if (!context ||
		context->selectedObjectType != EditorSelectionType::Object ||
		context->GetSelectedObjects().empty())
	{
		return false;
	}

	const Vector3 gizmoOrigin = GetTransformGizmoOrigin();
	const float gizmoScale = GetTransformGizmoScale(gizmoOrigin);
	const float axisLength = TransformGizmoAxisLength * gizmoScale;
	if (transformGizmoMode_ == EditorTransformGizmoMode::Scale)
	{
		const float centerPickRadius = TransformGizmoCenterPickRadius * gizmoScale;
		const float centerDistanceSquared = RaySegmentDistanceSquared(rayOrigin, rayDirection, gizmoOrigin, gizmoOrigin);
		if (centerDistanceSquared <= centerPickRadius * centerPickRadius)
		{
			outAxis = EditorTransformGizmoAxis::Center;
			return true;
		}
	}

	const float pickRadius = transformGizmoMode_ == EditorTransformGizmoMode::Rotate ?
		TransformGizmoRotationPickRadius * gizmoScale :
		TransformGizmoAxisPickRadius * gizmoScale;
	const float pickRadiusSquared = pickRadius * pickRadius;

	float closestDistanceSquared = MAX_FLOAT;
	for (EditorTransformGizmoAxis axis : { EditorTransformGizmoAxis::X, EditorTransformGizmoAxis::Y, EditorTransformGizmoAxis::Z })
	{
		const Vector3 axisDirection = GetTransformGizmoAxisVector(axis);
		const float distanceSquared = transformGizmoMode_ == EditorTransformGizmoMode::Rotate ?
			GetRotationRingDistanceSquared(
				rayOrigin,
				rayDirection,
				gizmoOrigin,
				axisDirection,
				TransformGizmoRotationRadius * gizmoScale) :
			RaySegmentDistanceSquared(rayOrigin, rayDirection, gizmoOrigin, gizmoOrigin + axisDirection * axisLength);
		if (distanceSquared <= pickRadiusSquared && distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			outAxis = axis;
		}
	}

	return outAxis != EditorTransformGizmoAxis::None;
}

bool ViewportPanel::GetTransformGizmoRotationDirectionUnderRay(
	const Vector3& rayOrigin,
	const Vector3& rayDirection,
	EditorTransformGizmoAxis axis,
	Vector3& outDirection) const
{
	outDirection = Vector3::ZeroVector;

	const Vector3 axisDirection = GetTransformGizmoAxisVector(axis);
	if (axisDirection.SquareLength() <= SMALLER_EPSILON)
	{
		return false;
	}

	const Vector3 gizmoOrigin = GetTransformGizmoOrigin();
	const float denominator = rayDirection.Dot(axisDirection);
	if (GoknarMath::Abs(denominator) <= SMALLER_EPSILON)
	{
		return false;
	}

	const float rayDistance = (gizmoOrigin - rayOrigin).Dot(axisDirection) / denominator;
	if (rayDistance < 0.f)
	{
		return false;
	}

	outDirection = rayOrigin + rayDirection * rayDistance - gizmoOrigin;
	if (outDirection.SquareLength() <= SMALLER_EPSILON)
	{
		return false;
	}

	outDirection.Normalize();
	return true;
}

Vector3 ViewportPanel::GetTransformGizmoOrigin() const
{
	EditorContext* context = EditorContext::Get();
	if (!context)
	{
		return Vector3::ZeroVector;
	}

	Vector3 origin = Vector3::ZeroVector;
	int selectedObjectCount = 0;
	for (ObjectBase* selectedObject : context->GetSelectedObjects())
	{
		if (selectedObject)
		{
			origin += selectedObject->GetWorldPosition();
			++selectedObjectCount;
		}
	}

	if (selectedObjectCount == 0)
	{
		return Vector3::ZeroVector;
	}

	return origin * (1.f / static_cast<float>(selectedObjectCount));
}

float ViewportPanel::GetTransformGizmoScale(const Vector3& origin) const
{
	EditorContext* context = EditorContext::Get();
	Camera* camera = context && context->viewportRenderTarget ? context->viewportRenderTarget->GetCamera() : nullptr;
	if (!camera)
	{
		return 1.f;
	}

	const float cameraDistance = Vector3::Distance(camera->GetPosition(), origin);
	return GoknarMath::Clamp(cameraDistance * TransformGizmoCameraDistanceScale, TransformGizmoMinScale, TransformGizmoMaxScale);
}

Vector3 ViewportPanel::GetTransformGizmoAxisVector(EditorTransformGizmoAxis axis) const
{
	switch (axis)
	{
	case EditorTransformGizmoAxis::X:
		return Vector3::ForwardVector;
	case EditorTransformGizmoAxis::Y:
		return Vector3::LeftVector;
	case EditorTransformGizmoAxis::Z:
		return Vector3::UpVector;
	default:
		return Vector3::ZeroVector;
	}
}

void ViewportPanel::SetTransformGizmoMode(EditorTransformGizmoMode mode)
{
	if (transformGizmoMode_ == mode)
	{
		return;
	}

	EndTransformGizmoDrag(false);
	transformGizmoMode_ = mode;
}

void ViewportPanel::SetTransformGizmoPivotMode(EditorTransformGizmoPivotMode mode)
{
	if (transformGizmoPivotMode_ == mode)
	{
		return;
	}

	EndTransformGizmoDrag(false);
	transformGizmoPivotMode_ = mode;
}

bool ViewportPanel::ShouldUseCollectiveTransformPivot() const
{
	return transformGizmoPivotMode_ == EditorTransformGizmoPivotMode::CollectiveWorldCenter &&
		draggedTransformGizmoObjects_.size() > 1 &&
		(transformGizmoMode_ == EditorTransformGizmoMode::Rotate ||
			transformGizmoMode_ == EditorTransformGizmoMode::Scale);
}

void ViewportPanel::UpdateTransformGizmoModeShortcuts()
{
	ImGuiIO& io = ImGui::GetIO();
	if (!IsCursorInsideViewport() ||
		io.WantTextInput ||
		ImGui::IsAnyItemActive() ||
		io.KeyCtrl ||
		io.KeyAlt)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
	{
		SetTransformGizmoMode(EditorTransformGizmoMode::Translate);
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
	{
		SetTransformGizmoMode(EditorTransformGizmoMode::Rotate);
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
	{
		SetTransformGizmoMode(EditorTransformGizmoMode::Scale);
	}
}

void ViewportPanel::DrawTransformGizmoModeToolbar()
{
	const ImVec2 previousCursorPosition = ImGui::GetCursorScreenPos();
	const ImVec2 toolbarPosition((float)position_.x + 8.f, (float)position_.y + 8.f);
	ImGui::SetCursorScreenPos(toolbarPosition);

	ImGui::PushID("TransformGizmoModeToolbar");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.f, 3.f));
	ImGui::BeginGroup();

	auto drawModeButton = [this](const char* label, EditorTransformGizmoMode mode)
		{
			const bool isSelected = transformGizmoMode_ == mode;
			if (isSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.36f, 0.62f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.44f, 0.72f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.28f, 0.52f, 1.f));
			}

			if (ImGui::Button(label))
			{
				SetTransformGizmoMode(mode);
			}

			if (isSelected)
			{
				ImGui::PopStyleColor(3);
			}
		};

	drawModeButton("Move", EditorTransformGizmoMode::Translate);
	ImGui::SameLine();
	drawModeButton("Rotate", EditorTransformGizmoMode::Rotate);
	ImGui::SameLine();
	drawModeButton("Scale", EditorTransformGizmoMode::Scale);

	EditorContext* context = EditorContext::Get();
	const bool shouldDrawPivotMode = context &&
		context->selectedObjectType == EditorSelectionType::Object &&
		context->GetSelectedObjects().size() > 1 &&
		transformGizmoMode_ != EditorTransformGizmoMode::Translate;
	if (shouldDrawPivotMode)
	{
		auto drawPivotButton = [this](const char* label, EditorTransformGizmoPivotMode mode)
			{
				const bool isSelected = transformGizmoPivotMode_ == mode;
				if (isSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.36f, 0.62f, 1.f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.44f, 0.72f, 1.f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.28f, 0.52f, 1.f));
				}

				if (ImGui::Button(label))
				{
					SetTransformGizmoPivotMode(mode);
				}

				if (isSelected)
				{
					ImGui::PopStyleColor(3);
				}
			};

		ImGui::Spacing();
		drawPivotButton("World Center", EditorTransformGizmoPivotMode::CollectiveWorldCenter);
		ImGui::SameLine();
		drawPivotButton("Local Center", EditorTransformGizmoPivotMode::LocalCenter);
	}

	ImGui::EndGroup();
	const ImVec2 toolbarMin = ImGui::GetItemRectMin();
	const ImVec2 toolbarMax = ImGui::GetItemRectMax();
	const ImVec2 mousePosition = ImGui::GetMousePos();
	isTransformGizmoModeToolbarHovered_ =
		toolbarMin.x <= mousePosition.x && mousePosition.x <= toolbarMax.x &&
		toolbarMin.y <= mousePosition.y && mousePosition.y <= toolbarMax.y;

	ImGui::PopStyleVar(2);
	ImGui::PopID();
	ImGui::SetCursorScreenPos(previousCursorPosition);
	ImGui::Dummy(ImVec2(0.f, 0.f));
}

void ViewportPanel::DrawTransformGizmo(Camera* camera) const
{
	EditorContext* context = EditorContext::Get();
	if (!camera ||
		!context ||
		context->selectedObjectType != EditorSelectionType::Object ||
		context->GetSelectedObjects().empty())
	{
		return;
	}

	const Vector3 gizmoOrigin = GetTransformGizmoOrigin();
	const float gizmoScale = GetTransformGizmoScale(gizmoOrigin);
	const float axisLength = TransformGizmoAxisLength * gizmoScale;
	const float ringRadius = TransformGizmoRotationRadius * gizmoScale;

	auto getScreenPosition = [this, camera](const Vector3& worldPosition)
		{
			const Vector2i screenPosition = camera->GetScreenPositionOfWorldPosition(worldPosition);
			return ImVec2(
				(float)(position_.x + screenPosition.x),
				(float)(position_.y + size_.y - screenPosition.y));
		};

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 clipMin((float)position_.x, (float)position_.y);
	const ImVec2 clipMax((float)(position_.x + size_.x), (float)(position_.y + size_.y));
	drawList->PushClipRect(clipMin, clipMax, true);

	auto isAxisActive = [this](EditorTransformGizmoAxis axis)
		{
			return isDraggingTransformGizmo_ && selectedTransformGizmoAxis_ == axis;
		};

	auto drawAxis = [this, drawList, &getScreenPosition, &isAxisActive, gizmoOrigin, axisLength](EditorTransformGizmoAxis axis, bool drawScaleHandle)
		{
			const Vector3 axisEnd = gizmoOrigin + GetTransformGizmoAxisVector(axis) * axisLength;
			const ImVec2 start = getScreenPosition(gizmoOrigin);
			const ImVec2 end = getScreenPosition(axisEnd);
			const ImU32 color = GetTransformGizmoAxisColor(axis, isAxisActive(axis));
			drawList->AddLine(start, end, IM_COL32(0, 0, 0, 220), 5.f);
			drawList->AddLine(start, end, color, 3.f);

			if (drawScaleHandle)
			{
				const ImVec2 shadowMin(end.x - TransformGizmoScaleHandleSize - 1.f, end.y - TransformGizmoScaleHandleSize - 1.f);
				const ImVec2 shadowMax(end.x + TransformGizmoScaleHandleSize + 1.f, end.y + TransformGizmoScaleHandleSize + 1.f);
				drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 220));
				drawList->AddRectFilled(
					ImVec2(end.x - TransformGizmoScaleHandleSize, end.y - TransformGizmoScaleHandleSize),
					ImVec2(end.x + TransformGizmoScaleHandleSize, end.y + TransformGizmoScaleHandleSize),
					color);
			}
			else
			{
				drawList->AddCircleFilled(end, 5.f, color);
			}
		};

	auto drawScaleCenterHandle = [this, drawList, &getScreenPosition, &isAxisActive, gizmoOrigin]()
		{
			const ImVec2 center = getScreenPosition(gizmoOrigin);
			const ImU32 color = isAxisActive(EditorTransformGizmoAxis::Center) ?
				IM_COL32(255, 255, 255, 255) :
				IM_COL32(255, 220, 70, 255);
			const ImVec2 shadowMin(center.x - TransformGizmoCenterHandleSize - 1.f, center.y - TransformGizmoCenterHandleSize - 1.f);
			const ImVec2 shadowMax(center.x + TransformGizmoCenterHandleSize + 1.f, center.y + TransformGizmoCenterHandleSize + 1.f);
			drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 220));
			drawList->AddRectFilled(
				ImVec2(center.x - TransformGizmoCenterHandleSize, center.y - TransformGizmoCenterHandleSize),
				ImVec2(center.x + TransformGizmoCenterHandleSize, center.y + TransformGizmoCenterHandleSize),
				color);
		};

	auto drawRotationRing = [this, drawList, &getScreenPosition, &isAxisActive, gizmoOrigin, ringRadius](EditorTransformGizmoAxis axis)
		{
			const Vector3 axisDirection = GetTransformGizmoAxisVector(axis);
			Vector3 tangent;
			Vector3 bitangent;
			if (!GetTransformGizmoRingBasis(axisDirection, tangent, bitangent))
			{
				return;
			}

			const ImU32 color = GetTransformGizmoAxisColor(axis, isAxisActive(axis));
			Vector3 previousRingPoint = gizmoOrigin + tangent * ringRadius;
			for (int segmentIndex = 1; segmentIndex <= TransformGizmoRingSegmentCount; ++segmentIndex)
			{
				const float angle = (TWO_PI * static_cast<float>(segmentIndex)) / static_cast<float>(TransformGizmoRingSegmentCount);
				const Vector3 currentRingPoint = gizmoOrigin +
					(tangent * std::cos(angle) + bitangent * std::sin(angle)) * ringRadius;
				const ImVec2 start = getScreenPosition(previousRingPoint);
				const ImVec2 end = getScreenPosition(currentRingPoint);
				drawList->AddLine(start, end, IM_COL32(0, 0, 0, 220), 4.f);
				drawList->AddLine(start, end, color, 2.f);
				previousRingPoint = currentRingPoint;
			}
		};

	if (transformGizmoMode_ == EditorTransformGizmoMode::Rotate)
	{
		drawRotationRing(EditorTransformGizmoAxis::X);
		drawRotationRing(EditorTransformGizmoAxis::Y);
		drawRotationRing(EditorTransformGizmoAxis::Z);
	}
	else
	{
		const bool drawScaleHandles = transformGizmoMode_ == EditorTransformGizmoMode::Scale;
		drawAxis(EditorTransformGizmoAxis::X, drawScaleHandles);
		drawAxis(EditorTransformGizmoAxis::Y, drawScaleHandles);
		drawAxis(EditorTransformGizmoAxis::Z, drawScaleHandles);
		if (drawScaleHandles)
		{
			drawScaleCenterHandle();
		}
	}

	drawList->PopClipRect();
}

void ViewportPanel::DrawSelectedObjectHighlight() const
{
	EditorContext* context = EditorContext::Get();
	if (!context ||
		context->selectedObjectType != EditorSelectionType::Object ||
		!context->selectedObject)
	{
		return;
	}

	Camera* camera = context->viewportRenderTarget ? context->viewportRenderTarget->GetCamera() : nullptr;
	if (!camera)
	{
		return;
	}

	for (ObjectBase* selectedObject : context->GetSelectedObjects())
	{
		const EditorCollisionGameObjectPair* selectedCollisionGameObjectPair = GetSelectedCollisionGameObjectPair(selectedObject);
		if (selectedCollisionGameObjectPair)
		{
			DrawMeshColliderHighlight(camera, *selectedCollisionGameObjectPair);
		}
	}
}

void ViewportPanel::DrawMeshColliderHighlight(Camera* camera, const EditorCollisionGameObjectPair& collisionGameObjectPair) const
{
	if (!collisionGameObjectPair.meshUnit)
	{
		return;
	}

	const VertexArray* vertices = collisionGameObjectPair.meshUnit->GetVerticesPointer();
	const FaceArray* faces = collisionGameObjectPair.meshUnit->GetFacesPointer();
	if (!vertices || !faces || vertices->empty() || faces->empty())
	{
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 clipMin((float)position_.x, (float)position_.y);
	const ImVec2 clipMax((float)(position_.x + size_.x), (float)(position_.y + size_.y));
	drawList->PushClipRect(clipMin, clipMax, true);

	auto getScreenPosition = [this, camera, &collisionGameObjectPair](const Vector3& localPosition)
		{
			const Vector3 worldPosition = TransformPoint(collisionGameObjectPair.transformationMatrix, localPosition);
			const Vector2i screenPosition = camera->GetScreenPositionOfWorldPosition(worldPosition);
			return ImVec2(
				(float)(position_.x + screenPosition.x),
				(float)(position_.y + size_.y - screenPosition.y));
		};

	auto drawEdge = [drawList](const ImVec2& start, const ImVec2& end)
		{
			drawList->AddLine(start, end, SelectedObjectShadowColor, 3.f);
			drawList->AddLine(start, end, SelectedObjectHighlightColor, 1.f);
		};

	const size_t totalEdgeCount = faces->size() * 3;
	const size_t edgeStep = totalEdgeCount <= MaxMeshHighlightEdges ?
		1 :
		(totalEdgeCount + MaxMeshHighlightEdges - 1) / MaxMeshHighlightEdges;
	size_t edgeIndex = 0;
	size_t drawnEdgeCount = 0;

	for (const Face& face : *faces)
	{
		if (face.vertexIndices[0] >= vertices->size() ||
			face.vertexIndices[1] >= vertices->size() ||
			face.vertexIndices[2] >= vertices->size())
		{
			continue;
		}

		const ImVec2 vertex0 = getScreenPosition(vertices->at(face.vertexIndices[0]).position);
		const ImVec2 vertex1 = getScreenPosition(vertices->at(face.vertexIndices[1]).position);
		const ImVec2 vertex2 = getScreenPosition(vertices->at(face.vertexIndices[2]).position);

		if ((edgeIndex++ % edgeStep) == 0 && drawnEdgeCount++ < MaxMeshHighlightEdges)
		{
			drawEdge(vertex0, vertex1);
		}
		if ((edgeIndex++ % edgeStep) == 0 && drawnEdgeCount++ < MaxMeshHighlightEdges)
		{
			drawEdge(vertex1, vertex2);
		}
		if ((edgeIndex++ % edgeStep) == 0 && drawnEdgeCount++ < MaxMeshHighlightEdges)
		{
			drawEdge(vertex2, vertex0);
		}

		if (MaxMeshHighlightEdges <= drawnEdgeCount)
		{
			break;
		}
	}

	drawList->PopClipRect();
}
