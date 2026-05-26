#include "NavigationPanel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Color.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Engine.h"
#include "Goknar/Application.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Math/Matrix.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Navigation/NavigationMesh.h"
#include "Goknar/Navigation/NavigationTreeSerializer.h"
#include "Goknar/Navigation/PathFinder.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Renderer/RenderTarget.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/ViewportPanel.h"

namespace
{
	constexpr float kVisualZOffset = 0.03f;
	constexpr float kPointHandleRadius = 0.18f;
	constexpr float kSelectedPointHandleRadius = 0.24f;
	constexpr float kPointHandlePickRadius = 0.32f;
	constexpr float kGizmoArrowLength = 1.35f;
	constexpr float kGizmoArrowThickness = 0.08f;
	constexpr float kGizmoAxisPickRadius = 0.18f;

	const Vector4 kDefaultNodeColor{ 0.12f, 0.65f, 0.20f, 0.28f };
	const Vector4 kSelectedNodeColor{ 0.62f, 1.00f, 0.58f, 0.48f };
	const Vector4 kNeighbourNodeColor{ 0.22f, 0.72f, 0.22f, 0.38f };
	const Vector4 kLineColor{ 0.18f, 0.90f, 0.18f, 0.90f };
	const Vector4 kCornerHandleColor{ 1.00f, 0.82f, 0.18f, 1.00f };
	const Vector4 kSelectedCornerHandleColor{ 1.00f, 1.00f, 0.82f, 1.00f };
	const Vector4 kGizmoXAxisColor{ 1.00f, 0.12f, 0.10f, 1.00f };
	const Vector4 kGizmoYAxisColor{ 0.20f, 0.95f, 0.20f, 1.00f };
	const Vector4 kGizmoZAxisColor{ 0.15f, 0.40f, 1.00f, 1.00f };

	class NavigationDebugObject : public ObjectBase
	{
	public:
		using ObjectBase::SetWorldTransformationMatrix;
	};

	struct NavigationEdge
	{
		Vector3 start;
		Vector3 end;
		Vector3 center;
	};

	const Vector3& GetAreaPoint(const Area& area, int pointIndex)
	{
		switch (pointIndex)
		{
		case 0: return area.point0;
		case 1: return area.point1;
		case 2: return area.point2;
		case 3: return area.point3;
		default: return area.point0;
		}
	}

	Vector3& GetMutableAreaPoint(Area& area, int pointIndex)
	{
		switch (pointIndex)
		{
		case 0: return area.point0;
		case 1: return area.point1;
		case 2: return area.point2;
		case 3: return area.point3;
		default: return area.point0;
		}
	}

	Vector3 GetAreaCenter(const Area& area)
	{
		return (area.point0 + area.point1 + area.point2 + area.point3) * 0.25f;
	}

	Vector3 GetAreaNormal(const Area& area)
	{
		Vector3 normal = (area.point1 - area.point0).Cross(area.point2 - area.point0);
		if (normal.SquareLength() <= SMALLER_EPSILON)
		{
			normal = (area.point2 - area.point0).Cross(area.point3 - area.point0);
		}

		if (normal.SquareLength() <= SMALLER_EPSILON)
		{
			return Vector3::UpVector;
		}

		return normal.GetNormalized();
	}

	Vector3 GetPointGizmoOrigin(const Area& area, int pointIndex)
	{
		return GetAreaPoint(area, pointIndex) + GetAreaNormal(area) * (kVisualZOffset * 4.f);
	}

	Vector3 GetGizmoAxisVector(NavigationPointGizmoAxis axis)
	{
		switch (axis)
		{
		case NavigationPointGizmoAxis::X:
			return Vector3::ForwardVector;
		case NavigationPointGizmoAxis::Y:
			return Vector3::LeftVector;
		case NavigationPointGizmoAxis::Z:
			return Vector3::UpVector;
		default:
			return Vector3::ZeroVector;
		}
	}

	const Vector4& GetGizmoAxisColor(NavigationPointGizmoAxis axis)
	{
		switch (axis)
		{
		case NavigationPointGizmoAxis::X:
			return kGizmoXAxisColor;
		case NavigationPointGizmoAxis::Y:
			return kGizmoYAxisColor;
		case NavigationPointGizmoAxis::Z:
			return kGizmoZAxisColor;
		default:
			return kLineColor;
		}
	}

	Vector3 SampleArea(const Area& area, float xAlpha, float yAlpha)
	{
		const Vector3 bottom = GoknarMath::Lerp(area.point0, area.point1, xAlpha);
		const Vector3 top = GoknarMath::Lerp(area.point3, area.point2, xAlpha);
		return GoknarMath::Lerp(bottom, top, yAlpha);
	}

	Vector2 ToHorizontalVector2(const Vector3& value)
	{
		return Vector2(value.x, value.y);
	}

	float GetTriangleSign(const Vector2& first, const Vector2& second, const Vector2& third)
	{
		return (first.x - third.x) * (second.y - third.y) - (second.x - third.x) * (first.y - third.y);
	}

	bool IsPointInTriangle(const Vector2& point, const Vector2& first, const Vector2& second, const Vector2& third)
	{
		const float sign0 = GetTriangleSign(point, first, second);
		const float sign1 = GetTriangleSign(point, second, third);
		const float sign2 = GetTriangleSign(point, third, first);

		const bool hasNegative = sign0 < 0.f || sign1 < 0.f || sign2 < 0.f;
		const bool hasPositive = 0.f < sign0 || 0.f < sign1 || 0.f < sign2;
		return !(hasNegative && hasPositive);
	}

	bool AreaContainsHorizontalPoint(const Area& area, const Vector3& position)
	{
		const Vector2 point = ToHorizontalVector2(position);
		const Vector2 point0 = ToHorizontalVector2(area.point0);
		const Vector2 point1 = ToHorizontalVector2(area.point1);
		const Vector2 point2 = ToHorizontalVector2(area.point2);
		const Vector2 point3 = ToHorizontalVector2(area.point3);
		return IsPointInTriangle(point, point0, point1, point2) || IsPointInTriangle(point, point0, point2, point3);
	}

	Vector3 GetEdgeCenter(const Area& area, int edgeIndex)
	{
		const Vector3& start = GetAreaPoint(area, edgeIndex);
		const Vector3& end = GetAreaPoint(area, (edgeIndex + 1) % 4);
		return (start + end) * 0.5f;
	}

	NavigationEdge GetAreaEdge(const Area& area, int edgeIndex)
	{
		const Vector3& start = GetAreaPoint(area, edgeIndex);
		const Vector3& end = GetAreaPoint(area, (edgeIndex + 1) % 4);
		return NavigationEdge{ start, end, (start + end) * 0.5f };
	}

	void GetClosestEdgeCenters(const Area& firstArea, const Area& secondArea, Vector3& outFirstEdgeCenter, Vector3& outSecondEdgeCenter)
	{
		float closestDistance = MAX_FLOAT;
		outFirstEdgeCenter = GetEdgeCenter(firstArea, 0);
		outSecondEdgeCenter = GetEdgeCenter(secondArea, 0);

		for (int firstEdgeIndex = 0; firstEdgeIndex < 4; ++firstEdgeIndex)
		{
			const Vector3 firstEdgeCenter = GetEdgeCenter(firstArea, firstEdgeIndex);
			for (int secondEdgeIndex = 0; secondEdgeIndex < 4; ++secondEdgeIndex)
			{
				const Vector3 secondEdgeCenter = GetEdgeCenter(secondArea, secondEdgeIndex);
				const float distance = Vector3::SquareDistance(firstEdgeCenter, secondEdgeCenter);
				if (distance < closestDistance)
				{
					closestDistance = distance;
					outFirstEdgeCenter = firstEdgeCenter;
					outSecondEdgeCenter = secondEdgeCenter;
				}
			}
		}
	}

	float Cross2D(const Vector2& first, const Vector2& second)
	{
		return first.x * second.y - first.y * second.x;
	}

	float PointSegmentDistance2D(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd)
	{
		const Vector2 segment = segmentEnd - segmentStart;
		const float segmentLengthSquared = segment.x * segment.x + segment.y * segment.y;
		if (segmentLengthSquared <= SMALLER_EPSILON)
		{
			return (point - segmentStart).Length();
		}

		const Vector2 pointToStart = point - segmentStart;
		const float t = GoknarMath::Clamp(Vector2::Dot(pointToStart, segment) / segmentLengthSquared, 0.f, 1.f);
		const Vector2 closestPoint = segmentStart + segment * t;
		return (point - closestPoint).Length();
	}

	bool SegmentsIntersect2D(const Vector2& firstStart, const Vector2& firstEnd, const Vector2& secondStart, const Vector2& secondEnd)
	{
		const Vector2 firstSegment = firstEnd - firstStart;
		const Vector2 secondSegment = secondEnd - secondStart;
		const float denominator = Cross2D(firstSegment, secondSegment);
		if (GoknarMath::Abs(denominator) <= SMALLER_EPSILON)
		{
			return false;
		}

		const Vector2 startDelta = secondStart - firstStart;
		const float t = Cross2D(startDelta, secondSegment) / denominator;
		const float u = Cross2D(startDelta, firstSegment) / denominator;
		return 0.f <= t && t <= 1.f && 0.f <= u && u <= 1.f;
	}

	float SegmentSegmentDistance2D(const NavigationEdge& firstEdge, const NavigationEdge& secondEdge)
	{
		const Vector2 firstStart = ToHorizontalVector2(firstEdge.start);
		const Vector2 firstEnd = ToHorizontalVector2(firstEdge.end);
		const Vector2 secondStart = ToHorizontalVector2(secondEdge.start);
		const Vector2 secondEnd = ToHorizontalVector2(secondEdge.end);

		if (SegmentsIntersect2D(firstStart, firstEnd, secondStart, secondEnd))
		{
			return 0.f;
		}

		return GoknarMath::Min(
			GoknarMath::Min(PointSegmentDistance2D(firstStart, secondStart, secondEnd), PointSegmentDistance2D(firstEnd, secondStart, secondEnd)),
			GoknarMath::Min(PointSegmentDistance2D(secondStart, firstStart, firstEnd), PointSegmentDistance2D(secondEnd, firstStart, firstEnd)));
	}

	float GetMinHorizontalEdgeDistanceAndStepHeight(const Area& firstArea, const Area& secondArea, float& outStepHeight)
	{
		float minDistance = MAX_FLOAT;
		outStepHeight = MAX_FLOAT;

		for (int firstEdgeIndex = 0; firstEdgeIndex < 4; ++firstEdgeIndex)
		{
			const NavigationEdge firstEdge = GetAreaEdge(firstArea, firstEdgeIndex);
			for (int secondEdgeIndex = 0; secondEdgeIndex < 4; ++secondEdgeIndex)
			{
				const NavigationEdge secondEdge = GetAreaEdge(secondArea, secondEdgeIndex);
				const float horizontalDistance = SegmentSegmentDistance2D(firstEdge, secondEdge);
				if (horizontalDistance < minDistance)
				{
					minDistance = horizontalDistance;
					outStepHeight = GoknarMath::Abs(firstEdge.center.z - secondEdge.center.z);
				}
			}
		}

		return minDistance;
	}

	bool IsNeighbour(const NavigationNode* node, const NavigationNode* candidate)
	{
		return node && candidate &&
			std::find(node->neighbours.begin(), node->neighbours.end(), candidate) != node->neighbours.end();
	}

	bool ContainsNavigationNode(const NavigationTree* navigationTree, const NavigationNode* navigationNode)
	{
		if (!navigationTree || !navigationNode)
		{
			return false;
		}

		for (const std::unique_ptr<NavigationNode>& node : navigationTree->GetNodes())
		{
			if (node.get() == navigationNode)
			{
				return true;
			}
		}

		return false;
	}

	bool RayIntersectsSphere(const Vector3& rayOrigin, const Vector3& rayDirection, const Vector3& sphereCenter, float radius, float& outRayDistance)
	{
		const Vector3 originToCenter = rayOrigin - sphereCenter;
		const float b = 2.f * originToCenter.Dot(rayDirection);
		const float c = originToCenter.Dot(originToCenter) - radius * radius;
		const float discriminant = b * b - 4.f * c;
		if (discriminant < 0.f)
		{
			return false;
		}

		const float sqrtDiscriminant = std::sqrt(discriminant);
		float rayDistance = (-b - sqrtDiscriminant) * 0.5f;
		if (rayDistance < 0.f)
		{
			rayDistance = (-b + sqrtDiscriminant) * 0.5f;
		}

		if (rayDistance < 0.f)
		{
			return false;
		}

		outRayDistance = rayDistance;
		return true;
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

	void CreateNavigationDebugMaterials(StaticMesh* mesh, const Vector4& baseColor, std::vector<Material*>& outMaterials)
	{
		outMaterials.clear();
		if (!mesh)
		{
			return;
		}

		const std::vector<MeshUnit*>& subMeshes = mesh->GetSubMeshes();
		outMaterials.reserve(subMeshes.size());
		for (MeshUnit* subMesh : subMeshes)
		{
			if (!subMesh)
			{
				outMaterials.push_back(nullptr);
				continue;
			}

			Material* material = new Material();
			material->SetBaseColor(baseColor);
			material->SetBlendModel(MaterialBlendModel::Transparent);
			material->SetShadingModel(MaterialShadingModel::TwoSided);
			material->SetTranslucency(baseColor.w);
			material->SetRoughness(0.8f);
			material->SetMetallic(0.f);

			subMesh->SetMaterial(material);

			outMaterials.push_back(material);
		}
	}

	void CopyStringToBuffer(const std::string& value, std::array<char, 512>& buffer)
	{
		buffer.fill('\0');
		const size_t copyLength = GoknarMath::Min(value.size(), buffer.size() - 1);
		std::memcpy(buffer.data(), value.c_str(), copyLength);
	}
}

NavigationPanel::NavigationPanel(EditorHUD* hud) :
	IEditorPanel("Navigation Editor", hud)
{
	isOpen_ = false;
	navigationTreePath_ = GetDefaultNavigationTreePath();
	CopyStringToBuffer(navigationTreePath_, navigationTreePathBuffer_);

	ResourceManager* resourceManager = engine ? engine->GetResourceManager() : nullptr;
	if (!resourceManager)
	{
		return;
	}

	triangleMesh_ = resourceManager->GetEngineContent<StaticMesh>("Navigation/Meshes/SM_Triangle.fbx");
	lineMesh_ = resourceManager->GetEngineContent<StaticMesh>("Navigation/Meshes/SM_Line.fbx");
	cornerSphereMesh_ = resourceManager->GetEngineContent<StaticMesh>("Navigation/Meshes/SM_LowPolyUnitSphere.fbx");
	gizmoArrowMesh_ = resourceManager->GetEngineContent<StaticMesh>("Navigation/Meshes/SM_LowPolyArrow.fbx");

	CreateNavigationDebugMaterials(triangleMesh_, kDefaultNodeColor, triangleSourceMaterials_);
	CreateNavigationDebugMaterials(lineMesh_, kLineColor, lineSourceMaterials_);
	CreateNavigationDebugMaterials(cornerSphereMesh_, kCornerHandleColor, cornerSphereSourceMaterials_);
	CreateNavigationDebugMaterials(gizmoArrowMesh_, kGizmoXAxisColor, gizmoArrowSourceMaterials_);
}

NavigationPanel::~NavigationPanel()
{
	ClearDebugVisuals();
	ClearPathDebugDraws();

	delete navigationTree_;
}

void NavigationPanel::Init()
{
}

void NavigationPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);
	const ImVec2 windowPosition = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	position_ = Vector2i((int)windowPosition.x, (int)windowPosition.y);
	size_ = Vector2i((int)windowSize.x, (int)windowSize.y);

	DrawToolbar();
	ValidateSelectedNode();
	ImGui::Separator();
	DrawSettings();
	ImGui::Separator();
	DrawPathFinder();
	ImGui::Separator();
	DrawNodeList();
	ImGui::Separator();
	DrawSelectedNodeProperties();

	if (!statusText_.empty())
	{
		ImGui::Separator();
		ImGui::TextWrapped("%s", statusText_.c_str());
	}

	ImGui::End();

	if (!isOpen_)
	{
		pathPointPickIndex_ = -1;
		EndPointGizmoDrag(false);
		ClearDebugVisuals();
		ClearPathDebugDraws();
		return;
	}

	UpdatePointGizmoDrag();
	RebuildDebugVisuals();
}

void NavigationPanel::SetIsOpen(bool isOpen)
{
	if (isOpen_ == isOpen)
	{
		return;
	}

	isOpen_ = isOpen;
	if (!isOpen_)
	{
		pathPointPickIndex_ = -1;
		EndPointGizmoDrag(false);
		ClearPointGizmoSelection();
		ClearDebugVisuals();
		ClearPathDebugDraws();
	}
	else
	{
		if (IsUsingSceneNavigationMesh())
		{
			RebuildSceneNavigationMesh();
		}

		MarkVisualsDirty();
	}
}

bool NavigationPanel::HandleViewportLeftClick()
{
	if (!isOpen_)
	{
		return false;
	}

	if (IsItemHovered())
	{
		return true;
	}

	if (!IsCursorInsideViewport())
	{
		return false;
	}

	if (pathPointPickIndex_ != -1)
	{
		return HandlePathPointPickClick();
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return false;
	}

	if (CanEditActiveNavigationTree() && TryHandlePointGizmoClick(rayOrigin, rayDirection))
	{
		return true;
	}

	Vector3 hitPosition;
	if (!TryGetViewportRaycastHit(hitPosition))
	{
		return false;
	}

	if (NavigationNode* clickedNode = FindNodeAtPosition(hitPosition))
	{
		SelectNode(clickedNode);
		ClearPointGizmoSelection();
		return true;
	}

	if (CanEditActiveNavigationTree() && editorMode_ == EditorMode::Draw)
	{
		AddNodeAtPosition(hitPosition);
		return true;
	}

	SelectNode(nullptr);
	ClearPointGizmoSelection();
	return true;
}

std::string NavigationPanel::GetDefaultNavigationTreePath() const
{
	return ContentDir + std::string("Navigation/NavigationTree");
}

NavigationTree* NavigationPanel::GetSceneNavigationTree() const
{
	Scene* scene = engine && engine->GetApplication() ? engine->GetApplication()->GetMainScene() : nullptr;
	NavigationMesh* navigationMesh = scene ? scene->GetNavigationMesh() : nullptr;
	return navigationMesh ? &navigationMesh->GetNavigationTree() : nullptr;
}

NavigationTree* NavigationPanel::GetActiveNavigationTree()
{
	if (useSceneNavigationMesh_)
	{
		if (NavigationTree* sceneNavigationTree = GetSceneNavigationTree())
		{
			return sceneNavigationTree;
		}
	}

	return navigationTree_;
}

const NavigationTree* NavigationPanel::GetActiveNavigationTree() const
{
	if (useSceneNavigationMesh_)
	{
		if (NavigationTree* sceneNavigationTree = GetSceneNavigationTree())
		{
			return sceneNavigationTree;
		}
	}

	return navigationTree_;
}

bool NavigationPanel::IsUsingSceneNavigationMesh() const
{
	return useSceneNavigationMesh_ && GetSceneNavigationTree() != nullptr;
}

bool NavigationPanel::CanEditActiveNavigationTree() const
{
	return GetActiveNavigationTree() != nullptr;
}

void NavigationPanel::RebuildSceneNavigationMesh(bool updateStatus)
{
	Scene* scene = engine && engine->GetApplication() ? engine->GetApplication()->GetMainScene() : nullptr;
	if (!scene)
	{
		if (updateStatus)
		{
			statusText_ = "Scene NavigationMesh rebuild failed. No scene is loaded.";
		}
		return;
	}

	scene->RebuildNavigationMesh(navMeshSettings_);
	selectedNode_ = nullptr;
	ClearPointGizmoSelection();
	ClearPathDebugDraws();
	MarkVisualsDirty();

	if (updateStatus)
	{
		statusText_ = "Scene NavigationMesh rebuilt.";
	}
}

void NavigationPanel::ValidateSelectedNode()
{
	if (ContainsNavigationNode(GetActiveNavigationTree(), selectedNode_))
	{
		return;
	}

	selectedNode_ = nullptr;
	ClearPointGizmoSelection();
}

void NavigationPanel::DrawToolbar()
{
	bool useSceneNavigationMesh = useSceneNavigationMesh_;
	if (ImGui::Checkbox("Use Scene NavigationMesh", &useSceneNavigationMesh))
	{
		useSceneNavigationMesh_ = useSceneNavigationMesh;
		editorMode_ = EditorMode::Select;
		selectedNode_ = nullptr;
		ClearPointGizmoSelection();
		ClearPathDebugDraws();

		if (IsUsingSceneNavigationMesh())
		{
			RebuildSceneNavigationMesh();
		}

		MarkVisualsDirty();
	}

	if (IsUsingSceneNavigationMesh())
	{
		NavigationTree* sceneNavigationTree = GetSceneNavigationTree();
		ImGui::Text("Source: Scene NavigationMesh");
		ImGui::Text("Nodes: %d", sceneNavigationTree ? (int)sceneNavigationTree->GetNodes().size() : 0);
	}

	if (ImGui::InputText("Tree Path", navigationTreePathBuffer_.data(), navigationTreePathBuffer_.size()))
	{
		navigationTreePath_ = navigationTreePathBuffer_.data();
	}

	if (ImGui::RadioButton("Select", editorMode_ == EditorMode::Select))
	{
		editorMode_ = EditorMode::Select;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Draw", editorMode_ == EditorMode::Draw))
	{
		editorMode_ = EditorMode::Draw;
	}

	if (ImGui::Button("Save"))
	{
		NavigationTree* activeNavigationTree = GetActiveNavigationTree();
		std::filesystem::path savePath = ContentPathUtils::ToAbsoluteContentPath(navigationTreePath_);
		if (savePath.has_parent_path())
		{
			std::error_code errorCode;
			std::filesystem::create_directories(savePath.parent_path(), errorCode);
		}

		if (activeNavigationTree && NavigationTreeSerializer::Serialize(navigationTreePath_, *activeNavigationTree))
		{
			statusText_ = "NavigationTree saved.";
			EditorContext::Get()->BuildFileTree();
		}
		else
		{
			statusText_ = "NavigationTree save failed.";
		}
	}
	ImGui::SameLine();
	if (!IsUsingSceneNavigationMesh())
	{
		if (ImGui::Button("Load"))
		{
			if (NavigationTreeSerializer::Deserialize(navigationTreePath_, *navigationTree_))
			{
				selectedNode_ = navigationTree_->GetRoot();
				ClearPointGizmoSelection();
				ClearPathDebugDraws();
				statusText_ = "NavigationTree loaded.";
				MarkVisualsDirty();
			}
			else
			{
				statusText_ = "NavigationTree load failed.";
			}
		}
		ImGui::SameLine();
	}

	if (ImGui::Button("Clear"))
	{
		NavigationTree* activeNavigationTree = GetActiveNavigationTree();
		if (activeNavigationTree)
		{
			activeNavigationTree->Clear();
			statusText_ = "NavigationTree cleared.";
		}
		else
		{
			statusText_ = "NavigationTree clear failed.";
		}
		selectedNode_ = nullptr;
		pathPointPickIndex_ = -1;
		ClearPointGizmoSelection();
		ClearPathDebugDraws();
		MarkVisualsDirty();
	}
}

void NavigationPanel::DrawSettings()
{
	bool rebuildNeighbours = false;

	ImGui::Text("Build Settings");
	rebuildNeighbours |= ImGui::InputFloat("Max Step Size", &navMeshSettings_.maxStepSize, 0.01f, 0.1f);
	ImGui::InputFloat("Max Slope Angle", &navMeshSettings_.maxSlopeAngle, 1.f, 5.f);
	rebuildNeighbours |= ImGui::InputFloat("Edge Connect Distance", &neighbourEdgeConnectionDistance_, 0.01f, 0.1f);
	ImGui::InputFloat("Default Node Half Extent", &defaultNodeHalfExtent_, 0.1f, 1.f);
	ImGui::InputFloat("Connection Line Thickness", &visualLineThickness_, 0.01f, 0.05f);

	navMeshSettings_.maxStepSize = GoknarMath::Max(navMeshSettings_.maxStepSize, 0.f);
	neighbourEdgeConnectionDistance_ = GoknarMath::Max(neighbourEdgeConnectionDistance_, 0.f);
	defaultNodeHalfExtent_ = GoknarMath::Max(defaultNodeHalfExtent_, 0.01f);
	visualLineThickness_ = GoknarMath::Max(visualLineThickness_, 0.005f);

	if (IsUsingSceneNavigationMesh())
	{
		if (ImGui::Button("Rebuild Neighbours") || rebuildNeighbours)
		{
			RebuildNeighbours();
		}
		ImGui::SameLine();
		if (ImGui::Button("Rebuild Scene Mesh"))
		{
			RebuildSceneNavigationMesh(true);
		}
	}
	else if (ImGui::Button("Rebuild Neighbours") || rebuildNeighbours)
	{
		RebuildNeighbours();
	}
}

void NavigationPanel::DrawPathFinder()
{
	ImGui::Text("Path Finder");
	DrawPathPointEditor(0, "Point 1");
	DrawPathPointEditor(1, "Point 2");

	if (ImGui::Button("Find Path"))
	{
		FindAndDrawPath();
	}

	if (pathPointPickIndex_ != -1)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Click the viewport to pick Point %d", pathPointPickIndex_ + 1);
	}
}

void NavigationPanel::DrawPathPointEditor(int pointIndex, const char* label)
{
	if (pointIndex < 0 || 1 < pointIndex)
	{
		return;
	}

	Vector3& point = pathPoints_[pointIndex];
	float pointValues[3] = { point.x, point.y, point.z };

	ImGui::Text("%s:", label);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.f);
	const std::string inputLabel = "##NavigationPathPoint" + std::to_string(pointIndex);
	if (ImGui::InputFloat3(inputLabel.c_str(), pointValues))
	{
		point.x = pointValues[0];
		point.y = pointValues[1];
		point.z = pointValues[2];
		hasPathPoint_[pointIndex] = true;
	}

	ImGui::SameLine();
	const std::string buttonLabel = "Pick##NavigationPathPoint" + std::to_string(pointIndex);
	if (ImGui::Button(buttonLabel.c_str()))
	{
		pathPointPickIndex_ = pathPointPickIndex_ == pointIndex ? -1 : pointIndex;
		EndPointGizmoDrag(false);
		ClearPointGizmoSelection();

		if (pathPointPickIndex_ != -1)
		{
			statusText_ = "Click the viewport to pick Point " + std::to_string(pathPointPickIndex_ + 1) + ".";
		}
	}
}

void NavigationPanel::DrawNodeList()
{
	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		ImGui::Text("Nodes: 0");
		return;
	}

	const auto& nodes = activeNavigationTree->GetNodes();
	ImGui::Text("Nodes: %d", (int)nodes.size());

	if (ImGui::BeginChild("NavigationNodes", ImVec2(0.f, 160.f), true))
	{
		for (const std::unique_ptr<NavigationNode>& node : nodes)
		{
			if (!node)
			{
				continue;
			}

			const bool isSelected = node.get() == selectedNode_;
			std::string label = "Node " + std::to_string(node->id) + " (" + std::to_string(node->neighbours.size()) + " neighbours)";
			if (ImGui::Selectable(label.c_str(), isSelected))
			{
				SelectNode(node.get());
			}
		}
	}
	ImGui::EndChild();

	if (CanEditActiveNavigationTree() && selectedNode_ && ImGui::Button("Delete Selected Node"))
	{
		DeleteSelectedNode();
	}
}

void NavigationPanel::DrawSelectedNodeProperties()
{
	if (!selectedNode_)
	{
		ImGui::Text("No NavigationNode selected.");
		return;
	}

	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	const bool canEditNavigationTree = CanEditActiveNavigationTree();

	ImGui::Text("Selected Node: %d", selectedNode_->id);
	ImGui::SameLine();
	if (activeNavigationTree && activeNavigationTree->GetRoot() == selectedNode_)
	{
		ImGui::TextDisabled("Root");
	}
	else if (canEditNavigationTree && ImGui::Button("Make Root"))
	{
		activeNavigationTree->SetRoot(selectedNode_);
	}

	bool areaChanged = false;
	for (int pointIndex = 0; pointIndex < 4; ++pointIndex)
	{
		Vector3& point = GetMutableAreaPoint(selectedNode_->area, pointIndex);
		float pointValues[3] = { point.x, point.y, point.z };
		const std::string label = "Point " + std::to_string(pointIndex);
		if (canEditNavigationTree)
		{
			if (ImGui::InputFloat3(label.c_str(), pointValues))
			{
				point.x = pointValues[0];
				point.y = pointValues[1];
				point.z = pointValues[2];
				areaChanged = true;
			}
		}
		else
		{
			ImGui::Text("%s: %.3f %.3f %.3f", label.c_str(), point.x, point.y, point.z);
		}
	}

	if (areaChanged)
	{
		RebuildNeighbours();
	}

	if (canEditNavigationTree)
	{
		ImGui::InputInt("Grid X", &gridDivisionCountX_);
		ImGui::InputInt("Grid Y", &gridDivisionCountY_);
		gridDivisionCountX_ = GoknarMath::Clamp(gridDivisionCountX_, 1, 64);
		gridDivisionCountY_ = GoknarMath::Clamp(gridDivisionCountY_, 1, 64);

		if (ImGui::Button("Divide Selected Node Into Grid"))
		{
			DivideSelectedNodeIntoGrid();
			return;
		}
	}

	ImGui::Text("Neighbours");
	if (selectedNode_->neighbours.empty())
	{
		ImGui::TextDisabled("None");
		return;
	}

	for (NavigationNode* neighbour : selectedNode_->neighbours)
	{
		if (!neighbour)
		{
			continue;
		}

		const std::string label = "Node " + std::to_string(neighbour->id);
		if (ImGui::Selectable(label.c_str()))
		{
			SelectNode(neighbour);
		}
	}
}

void NavigationPanel::AddNodeAtPosition(const Vector3& position)
{
	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return;
	}

	const float halfExtent = GoknarMath::Max(defaultNodeHalfExtent_, 0.01f);

	Area area;
	area.point0 = position + Vector3(-halfExtent, -halfExtent, 0.f);
	area.point1 = position + Vector3(halfExtent, -halfExtent, 0.f);
	area.point2 = position + Vector3(halfExtent, halfExtent, 0.f);
	area.point3 = position + Vector3(-halfExtent, halfExtent, 0.f);

	NavigationNode* node = activeNavigationTree->AddNode(area);
	SelectNode(node);
	RebuildNeighbours();
	statusText_ = "NavigationNode added.";
}

void NavigationPanel::SelectNode(NavigationNode* node)
{
	if (selectedNode_ == node)
	{
		return;
	}

	EndPointGizmoDrag(false);
	selectedNode_ = node;
	ClearPointGizmoSelection();
	MarkVisualsDirty();
}

NavigationNode* NavigationPanel::FindNodeAtPosition(const Vector3& position) const
{
	const NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return nullptr;
	}

	NavigationNode* closestNode = nullptr;
	float closestHeightDistance = MAX_FLOAT;

	for (const std::unique_ptr<NavigationNode>& node : activeNavigationTree->GetNodes())
	{
		if (!node || !AreaContainsHorizontalPoint(node->area, position))
		{
			continue;
		}

		const float heightDistance = GoknarMath::Abs(GetAreaCenter(node->area).z - position.z);
		if (heightDistance < closestHeightDistance)
		{
			closestHeightDistance = heightDistance;
			closestNode = node.get();
		}
	}

	return closestNode;
}

void NavigationPanel::DeleteSelectedNode()
{
	if (!CanEditActiveNavigationTree() || !selectedNode_)
	{
		return;
	}

	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return;
	}

	activeNavigationTree->RemoveNode(selectedNode_);
	selectedNode_ = nullptr;
	ClearPointGizmoSelection();
	ClearPathDebugDraws();
	RebuildNeighbours();
	statusText_ = "NavigationNode deleted.";
}

void NavigationPanel::DivideSelectedNodeIntoGrid()
{
	if (!CanEditActiveNavigationTree() || !selectedNode_)
	{
		return;
	}

	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return;
	}

	const int divisionsX = GoknarMath::Clamp(gridDivisionCountX_, 1, 64);
	const int divisionsY = GoknarMath::Clamp(gridDivisionCountY_, 1, 64);
	const Area sourceArea = selectedNode_->area;
	const bool wasRoot = activeNavigationTree->GetRoot() == selectedNode_;

	if ((sourceArea.point1 - sourceArea.point0).SquareLength() <= SMALLER_EPSILON ||
		(sourceArea.point3 - sourceArea.point0).SquareLength() <= SMALLER_EPSILON)
	{
		statusText_ = "Selected NavigationNode is too small to divide.";
		return;
	}

	activeNavigationTree->RemoveNode(selectedNode_);
	selectedNode_ = nullptr;
	ClearPointGizmoSelection();
	ClearPathDebugDraws();

	NavigationNode* firstCreatedNode = nullptr;
	for (int yIndex = 0; yIndex < divisionsY; ++yIndex)
	{
		const float minYAlpha = (float)yIndex / (float)divisionsY;
		const float maxYAlpha = (float)(yIndex + 1) / (float)divisionsY;

		for (int xIndex = 0; xIndex < divisionsX; ++xIndex)
		{
			const float minXAlpha = (float)xIndex / (float)divisionsX;
			const float maxXAlpha = (float)(xIndex + 1) / (float)divisionsX;

			Area cellArea;
			cellArea.point0 = SampleArea(sourceArea, minXAlpha, minYAlpha);
			cellArea.point1 = SampleArea(sourceArea, maxXAlpha, minYAlpha);
			cellArea.point2 = SampleArea(sourceArea, maxXAlpha, maxYAlpha);
			cellArea.point3 = SampleArea(sourceArea, minXAlpha, maxYAlpha);

			NavigationNode* node = activeNavigationTree->AddNode(cellArea);
			if (!firstCreatedNode)
			{
				firstCreatedNode = node;
			}
		}
	}

	if (wasRoot && firstCreatedNode)
	{
		activeNavigationTree->SetRoot(firstCreatedNode);
	}

	SelectNode(firstCreatedNode);
	RebuildNeighbours();
	statusText_ = "NavigationNode divided into " + std::to_string(divisionsX * divisionsY) + " grid nodes.";
}

void NavigationPanel::RebuildNeighbours()
{
	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return;
	}

	const float connectionDistance = GoknarMath::Max(neighbourEdgeConnectionDistance_, 0.f);
	const auto& nodes = activeNavigationTree->GetNodes();

	for (const std::unique_ptr<NavigationNode>& node : nodes)
	{
		if (node)
		{
			node->neighbours.clear();
		}
	}

	for (size_t firstNodeIndex = 0; firstNodeIndex < nodes.size(); ++firstNodeIndex)
	{
		NavigationNode* firstNode = nodes[firstNodeIndex].get();
		if (!firstNode)
		{
			continue;
		}

		for (size_t secondNodeIndex = firstNodeIndex + 1; secondNodeIndex < nodes.size(); ++secondNodeIndex)
		{
			NavigationNode* secondNode = nodes[secondNodeIndex].get();
			if (!secondNode)
			{
				continue;
			}

			float stepHeight = 0.f;
			const float edgeDistance = GetMinHorizontalEdgeDistanceAndStepHeight(firstNode->area, secondNode->area, stepHeight);
			if (edgeDistance <= connectionDistance && stepHeight <= navMeshSettings_.maxStepSize)
			{
				firstNode->neighbours.push_back(secondNode);
				secondNode->neighbours.push_back(firstNode);
			}
		}
	}

	MarkVisualsDirty();
}

bool NavigationPanel::GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const
{
	EditorContext* context = EditorContext::Get();
	if (!context || !context->viewportRenderTarget || !engine)
	{
		return false;
	}

	Camera* activeCamera = context->viewportRenderTarget->GetCamera();
	ViewportPanel* viewportPanel = hud_->GetPanel<ViewportPanel>();
	if (!activeCamera || !viewportPanel)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	const Vector2i& viewportPosition = viewportPanel->GetPosition();
	outOrigin = activeCamera->GetPosition();
	outDirection = activeCamera->GetWorldDirectionAtPixel(
		Vector2i{ (int)(xPosition - viewportPosition.x), (int)(yPosition - viewportPosition.y) }).GetNormalized();

	return outDirection.SquareLength() > SMALLER_EPSILON;
}

bool NavigationPanel::TryGetViewportRaycastHit(Vector3& outPosition) const
{
	EditorContext* context = EditorContext::Get();
	if (!context || !context->viewportRenderTarget || !engine || !engine->GetPhysicsWorld())
	{
		return false;
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return false;
	}

	RaycastData raycastData;
	raycastData.from = rayOrigin;
	raycastData.to = rayOrigin + rayDirection * 1000.f;
	raycastData.collisionGroup = CollisionGroup::All;
	raycastData.collisionMask = CollisionMask(0xFFFFFF);

	RaycastSingleResult raycastResult;
	if (!engine->GetPhysicsWorld()->RaycastClosest(raycastData, raycastResult))
	{
		return false;
	}

	outPosition = raycastResult.hitPosition;
	return true;
}

bool NavigationPanel::IsCursorInsideViewport() const
{
	ViewportPanel* viewportPanel = hud_->GetPanel<ViewportPanel>();
	if (!viewportPanel)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	const Vector2i& viewportPosition = viewportPanel->GetPosition();
	const Vector2i& viewportSize = viewportPanel->GetSize();
	return viewportPosition.x <= xPosition &&
		xPosition <= viewportPosition.x + viewportSize.x &&
		viewportPosition.y <= yPosition &&
		yPosition <= viewportPosition.y + viewportSize.y;
}

bool NavigationPanel::HandlePathPointPickClick()
{
	if (pathPointPickIndex_ < 0 || 1 < pathPointPickIndex_)
	{
		pathPointPickIndex_ = -1;
		return false;
	}

	Vector3 hitPosition;
	if (!TryGetViewportRaycastHit(hitPosition))
	{
		statusText_ = "Path point pick failed. Click a collidable point in the viewport.";
		return true;
	}

	pathPoints_[pathPointPickIndex_] = hitPosition;
	hasPathPoint_[pathPointPickIndex_] = true;
	statusText_ = "Point " + std::to_string(pathPointPickIndex_ + 1) + " picked.";
	pathPointPickIndex_ = -1;
	return true;
}

void NavigationPanel::FindAndDrawPath()
{
	if (!hasPathPoint_[0] || !hasPathPoint_[1])
	{
		statusText_ = "Pick Point 1 and Point 2 before finding a path.";
		return;
	}

	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		statusText_ = "Path not found. No navigation tree is available.";
		return;
	}

	PathFinder pathFinder(activeNavigationTree);
	std::vector<NavigationPath> path;
	if (!pathFinder.FindPath(pathPoints_[0], pathPoints_[1], path))
	{
		statusText_ = "Path not found. Make sure both points are inside connected navigation areas.";
		return;
	}

	ClearPathDebugDraws();
	DrawPathDebugLine(path);

	if (engine)
	{
		engine->InitializePendingObjectsAndComponents();
	}

	statusText_ = "Path found. Raw points: " + std::to_string((int)path.size()) + ".";
}

bool NavigationPanel::IsPathSegmentWalkable(const Vector3& start, const Vector3& end) const
{
	if (!FindNodeAtPosition(start) || !FindNodeAtPosition(end))
	{
		return false;
	}

	const Vector3 segment = end - start;
	const float segmentLength = segment.Length();
	if (segmentLength <= SMALLER_EPSILON)
	{
		return true;
	}

	const float sampleStep = GoknarMath::Max(neighbourEdgeConnectionDistance_ * 0.5f, 0.05f);
	const int sampleCount = GoknarMath::Max((int)std::ceil(segmentLength / sampleStep), 1);
	for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex)
	{
		const float alpha = (float)sampleIndex / (float)sampleCount;
		const Vector3 samplePosition = start + segment * alpha;
		if (!FindNodeAtPosition(samplePosition))
		{
			return false;
		}
	}

	return true;
}

void NavigationPanel::DrawPathDebugLine(const std::vector<NavigationPath>& path)
{
	if (path.size() < 2)
	{
		return;
	}

	if (!pathDebugOwner_)
	{
		pathDebugOwner_ = new ObjectBase();
		pathDebugOwner_->SetName("__Editor__NavigationPathDebug");
	}

	const Colorf pathColor = Colorf::Green;
	const Vector3 drawOffset(0.f, 0.f, kVisualZOffset * 8.f);

	for (size_t pathPointIndex = 1; pathPointIndex < path.size(); ++pathPointIndex)
	{
		DebugDrawer::DrawLine(
			path[pathPointIndex - 1].position + drawOffset,
			path[pathPointIndex].position + drawOffset,
			pathColor,
			2.f,
			-1.f,
			pathDebugOwner_);
	}
}

void NavigationPanel::ClearPathDebugDraws()
{
	if (pathDebugOwner_)
	{
		pathDebugOwner_->Destroy();
		pathDebugOwner_ = nullptr;
	}
}

void NavigationPanel::UpdatePointGizmoDrag()
{
	if (!isDraggingPoint_)
	{
		return;
	}

	if (!CanEditActiveNavigationTree())
	{
		EndPointGizmoDrag(false);
		return;
	}

	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		EndPointGizmoDrag(true);
		return;
	}

	if (!selectedNode_ || draggedNode_ != selectedNode_ || selectedPointIndex_ < 0 || 3 < selectedPointIndex_ ||
		selectedGizmoAxis_ == NavigationPointGizmoAxis::None)
	{
		EndPointGizmoDrag(false);
		return;
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return;
	}

	const Vector3 axisDirection = GetGizmoAxisVector(selectedGizmoAxis_);
	float currentAxisParameter = 0.f;
	if (!GetAxisParameterFromRay(rayOrigin, rayDirection, dragStartGizmoOrigin_, axisDirection, currentAxisParameter))
	{
		return;
	}

	Vector3& point = GetMutableAreaPoint(selectedNode_->area, selectedPointIndex_);
	point = dragStartPoint_ + axisDirection * (currentAxisParameter - dragStartAxisParameter_);
	MarkVisualsDirty();
}

void NavigationPanel::EndPointGizmoDrag(bool rebuildNeighbours)
{
	const bool wasDragging = isDraggingPoint_;
	isDraggingPoint_ = false;
	draggedNode_ = nullptr;
	selectedGizmoAxis_ = NavigationPointGizmoAxis::None;
	dragStartGizmoOrigin_ = Vector3::ZeroVector;

	if (wasDragging && rebuildNeighbours)
	{
		RebuildNeighbours();
		return;
	}

	if (wasDragging)
	{
		MarkVisualsDirty();
	}
}

void NavigationPanel::ClearPointGizmoSelection()
{
	const bool hadPointSelection = selectedPointIndex_ != -1 || selectedGizmoAxis_ != NavigationPointGizmoAxis::None || isDraggingPoint_;
	selectedPointIndex_ = -1;
	selectedGizmoAxis_ = NavigationPointGizmoAxis::None;
	isDraggingPoint_ = false;
	draggedNode_ = nullptr;
	dragStartGizmoOrigin_ = Vector3::ZeroVector;
	if (hadPointSelection)
	{
		MarkVisualsDirty();
	}
}

bool NavigationPanel::TryHandlePointGizmoClick(const Vector3& rayOrigin, const Vector3& rayDirection)
{
	if (!CanEditActiveNavigationTree() || !selectedNode_)
	{
		return false;
	}

	if (0 <= selectedPointIndex_ && selectedPointIndex_ < 4)
	{
		NavigationPointGizmoAxis clickedAxis = NavigationPointGizmoAxis::None;
		if (FindAxisUnderRay(rayOrigin, rayDirection, clickedAxis))
		{
			selectedGizmoAxis_ = clickedAxis;
			isDraggingPoint_ = true;
			draggedNode_ = selectedNode_;
			dragStartPoint_ = GetAreaPoint(selectedNode_->area, selectedPointIndex_);
			dragStartGizmoOrigin_ = GetPointGizmoOrigin(selectedNode_->area, selectedPointIndex_);

			const Vector3 axisDirection = GetGizmoAxisVector(selectedGizmoAxis_);
			if (!GetAxisParameterFromRay(rayOrigin, rayDirection, dragStartGizmoOrigin_, axisDirection, dragStartAxisParameter_))
			{
				dragStartAxisParameter_ = 0.f;
			}

			MarkVisualsDirty();
			return true;
		}
	}

	int clickedPointIndex = -1;
	if (FindPointHandleUnderRay(rayOrigin, rayDirection, clickedPointIndex))
	{
		selectedPointIndex_ = clickedPointIndex;
		selectedGizmoAxis_ = NavigationPointGizmoAxis::None;
		isDraggingPoint_ = false;
		draggedNode_ = nullptr;
		MarkVisualsDirty();
		return true;
	}

	return false;
}

bool NavigationPanel::FindPointHandleUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, int& outPointIndex) const
{
	if (!selectedNode_)
	{
		return false;
	}

	float closestRayDistance = MAX_FLOAT;
	outPointIndex = -1;
	for (int pointIndex = 0; pointIndex < 4; ++pointIndex)
	{
		float rayDistance = 0.f;
		if (!RayIntersectsSphere(rayOrigin, rayDirection, GetPointGizmoOrigin(selectedNode_->area, pointIndex), kPointHandlePickRadius, rayDistance))
		{
			continue;
		}

		if (rayDistance < closestRayDistance)
		{
			closestRayDistance = rayDistance;
			outPointIndex = pointIndex;
		}
	}

	return outPointIndex != -1;
}

bool NavigationPanel::FindAxisUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, NavigationPointGizmoAxis& outAxis) const
{
	if (!selectedNode_ || selectedPointIndex_ < 0 || 3 < selectedPointIndex_)
	{
		return false;
	}

	const Vector3 gizmoOrigin = GetPointGizmoOrigin(selectedNode_->area, selectedPointIndex_);
	const float pickRadiusSquared = kGizmoAxisPickRadius * kGizmoAxisPickRadius;
	float closestDistanceSquared = MAX_FLOAT;
	outAxis = NavigationPointGizmoAxis::None;

	for (NavigationPointGizmoAxis axis : { NavigationPointGizmoAxis::X, NavigationPointGizmoAxis::Y, NavigationPointGizmoAxis::Z })
	{
		const Vector3 axisDirection = GetGizmoAxisVector(axis);
		const Vector3 axisEnd = gizmoOrigin + axisDirection * kGizmoArrowLength;
		const float distanceSquared = RaySegmentDistanceSquared(rayOrigin, rayDirection, gizmoOrigin, axisEnd);
		if (distanceSquared <= pickRadiusSquared && distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			outAxis = axis;
		}
	}

	return outAxis != NavigationPointGizmoAxis::None;
}

void NavigationPanel::MarkVisualsDirty()
{
	areVisualsDirty_ = true;
}

void NavigationPanel::RebuildDebugVisuals()
{
	if (!areVisualsDirty_)
	{
		return;
	}

	areVisualsDirty_ = false;
	ClearDebugVisuals();
	ValidateSelectedNode();

	NavigationTree* activeNavigationTree = GetActiveNavigationTree();
	if (!activeNavigationTree)
	{
		return;
	}

	bool createdVisualObject = false;
	for (const std::unique_ptr<NavigationNode>& node : activeNavigationTree->GetNodes())
	{
		if (node)
		{
			CreateNodeVisual(node.get());
			createdVisualObject = true;
		}
	}

	for (const std::unique_ptr<NavigationNode>& node : activeNavigationTree->GetNodes())
	{
		if (!node)
		{
			continue;
		}

		for (NavigationNode* neighbour : node->neighbours)
		{
			if (!neighbour || node->id > neighbour->id)
			{
				continue;
			}

			CreateNeighbourVisual(node.get(), neighbour);
			createdVisualObject = true;
		}
	}

	if (CanEditActiveNavigationTree() && selectedNode_)
	{
		CreateSelectedNodePointVisuals();
		createdVisualObject = true;
	}

	if (createdVisualObject)
	{
		engine->InitializePendingObjectsAndComponents();
	}
}

void NavigationPanel::ClearDebugVisuals()
{
	for (ObjectBase* visualObject : debugVisualObjects_)
	{
		if (visualObject)
		{
			visualObject->Destroy();
		}
	}

	debugVisualObjects_.clear();
}

void NavigationPanel::CreateNodeVisual(NavigationNode* node)
{
	if (!node || !triangleMesh_)
	{
		return;
	}

	Vector4 color = kDefaultNodeColor;
	if (node == selectedNode_)
	{
		color = kSelectedNodeColor;
	}
	else if (IsNeighbour(selectedNode_, node))
	{
		color = kNeighbourNodeColor;
	}

	CreateNodeTriangleVisual(node, node->area.point0, node->area.point1, node->area.point2, color);
	CreateNodeTriangleVisual(node, node->area.point0, node->area.point2, node->area.point3, color);
}

void NavigationPanel::CreateNodeTriangleVisual(NavigationNode* node, const Vector3& point0, const Vector3& point1, const Vector3& point2, const Vector4& color)
{
	if (!node || !triangleMesh_)
	{
		return;
	}

	NavigationDebugObject* triangleObject = new NavigationDebugObject();
	triangleObject->SetName("__Editor__NavigationNodeTriangle");

	StaticMeshComponent* meshComponent = triangleObject->AddSubComponent<StaticMeshComponent>();
	meshComponent->SetMesh(triangleMesh_);

	for (int materialIndex = 0; materialIndex < (int)triangleSourceMaterials_.size(); ++materialIndex)
	{
		if (MaterialInstance* materialInstance = CreateMaterialInstance(triangleSourceMaterials_[materialIndex], color))
		{
			meshComponent->GetMeshInstance()->SetMaterial(materialIndex, materialInstance);
		}
	}
	meshComponent->GetMeshInstance()->SetIsCastingShadow(false);

	ApplyTriangleTransform(triangleObject, point0, point1, point2);
	debugVisualObjects_.push_back(triangleObject);
}

void NavigationPanel::CreateNeighbourVisual(const NavigationNode* firstNode, const NavigationNode* secondNode)
{
	if (!firstNode || !secondNode || !lineMesh_)
	{
		return;
	}

	Vector3 start;
	Vector3 end;
	GetClosestEdgeCenters(firstNode->area, secondNode->area, start, end);
	start.z += kVisualZOffset * 2.f;
	end.z += kVisualZOffset * 2.f;

	const Vector3 lineDirection = end - start;
	const float lineLength = lineDirection.Length();
	if (lineLength <= SMALLER_EPSILON)
	{
		return;
	}

	ObjectBase* lineObject = new ObjectBase();
	lineObject->SetName("__Editor__NavigationNodeNeighbourLine");

	StaticMeshComponent* meshComponent = lineObject->AddSubComponent<StaticMeshComponent>();
	meshComponent->SetMesh(lineMesh_);

	for (int materialIndex = 0; materialIndex < (int)lineSourceMaterials_.size(); ++materialIndex)
	{
		if (MaterialInstance* materialInstance = CreateMaterialInstance(lineSourceMaterials_[materialIndex], kLineColor))
		{
			meshComponent->GetMeshInstance()->SetMaterial(materialIndex, materialInstance);
		}
	}
	meshComponent->GetMeshInstance()->SetIsCastingShadow(false);

	lineObject->SetWorldPosition(start, false);
	lineObject->SetWorldScaling(Vector3(lineLength, visualLineThickness_, visualLineThickness_), false);
	lineObject->SetWorldRotation(Quaternion::FromTwoVectors(Vector3::ForwardVector, lineDirection.GetNormalized()));
	debugVisualObjects_.push_back(lineObject);
}

void NavigationPanel::CreateSelectedNodePointVisuals()
{
	if (!selectedNode_)
	{
		return;
	}

	for (int pointIndex = 0; pointIndex < 4; ++pointIndex)
	{
		CreatePointHandleVisual(pointIndex, pointIndex == selectedPointIndex_);
	}

	if (0 <= selectedPointIndex_ && selectedPointIndex_ < 4)
	{
		CreateAxisArrowVisual(NavigationPointGizmoAxis::X);
		CreateAxisArrowVisual(NavigationPointGizmoAxis::Y);
		CreateAxisArrowVisual(NavigationPointGizmoAxis::Z);
	}
}

void NavigationPanel::CreatePointHandleVisual(int pointIndex, bool isSelected)
{
	if (!selectedNode_ || !cornerSphereMesh_ || pointIndex < 0 || 3 < pointIndex)
	{
		return;
	}

	ObjectBase* handleObject = new ObjectBase();
	handleObject->SetName("__Editor__NavigationNodePointHandle");

	StaticMeshComponent* meshComponent = handleObject->AddSubComponent<StaticMeshComponent>();
	meshComponent->SetMesh(cornerSphereMesh_);

	const Vector4& color = isSelected ? kSelectedCornerHandleColor : kCornerHandleColor;
	for (int materialIndex = 0; materialIndex < (int)cornerSphereSourceMaterials_.size(); ++materialIndex)
	{
		if (MaterialInstance* materialInstance = CreateMaterialInstance(cornerSphereSourceMaterials_[materialIndex], color))
		{
			meshComponent->GetMeshInstance()->SetMaterial(materialIndex, materialInstance);
		}
	}
	meshComponent->GetMeshInstance()->SetIsCastingShadow(false);

	const float handleScale = isSelected ? kSelectedPointHandleRadius : kPointHandleRadius;
	handleObject->SetWorldPosition(GetPointGizmoOrigin(selectedNode_->area, pointIndex), false);
	handleObject->SetWorldScaling(Vector3(handleScale), false);
	handleObject->SetWorldRotation(Quaternion::Identity);
	debugVisualObjects_.push_back(handleObject);
}

void NavigationPanel::CreateAxisArrowVisual(NavigationPointGizmoAxis axis)
{
	if (!selectedNode_ || !gizmoArrowMesh_ || selectedPointIndex_ < 0 || 3 < selectedPointIndex_)
	{
		return;
	}

	const Vector3 axisDirection = GetGizmoAxisVector(axis);
	if (axisDirection.SquareLength() <= SMALLER_EPSILON)
	{
		return;
	}

	ObjectBase* arrowObject = new ObjectBase();
	arrowObject->SetName("__Editor__NavigationNodePointAxis");

	StaticMeshComponent* meshComponent = arrowObject->AddSubComponent<StaticMeshComponent>();
	meshComponent->SetMesh(gizmoArrowMesh_);

	const Vector4& color = GetGizmoAxisColor(axis);
	for (int materialIndex = 0; materialIndex < (int)gizmoArrowSourceMaterials_.size(); ++materialIndex)
	{
		if (MaterialInstance* materialInstance = CreateMaterialInstance(gizmoArrowSourceMaterials_[materialIndex], color))
		{
			meshComponent->GetMeshInstance()->SetMaterial(materialIndex, materialInstance);
		}
	}
	meshComponent->GetMeshInstance()->SetIsCastingShadow(false);

	const Vector3 origin = GetPointGizmoOrigin(selectedNode_->area, selectedPointIndex_);
	arrowObject->SetWorldPosition(origin, false);
	arrowObject->SetWorldScaling(Vector3(kGizmoArrowLength, kGizmoArrowThickness, kGizmoArrowThickness), false);
	arrowObject->SetWorldRotation(Quaternion::FromTwoVectors(Vector3::ForwardVector, axisDirection));
	debugVisualObjects_.push_back(arrowObject);
}

void NavigationPanel::CreateLineVisual(const char* objectName, const Vector3& start, const Vector3& end, const Vector4& color, float thickness)
{
	if (!lineMesh_)
	{
		return;
	}

	const Vector3 lineDirection = end - start;
	const float lineLength = lineDirection.Length();
	if (lineLength <= SMALLER_EPSILON)
	{
		return;
	}

	ObjectBase* lineObject = new ObjectBase();
	lineObject->SetName(objectName ? objectName : "__Editor__NavigationLine");

	StaticMeshComponent* meshComponent = lineObject->AddSubComponent<StaticMeshComponent>();
	meshComponent->SetMesh(lineMesh_);

	for (int materialIndex = 0; materialIndex < (int)lineSourceMaterials_.size(); ++materialIndex)
	{
		if (MaterialInstance* materialInstance = CreateMaterialInstance(lineSourceMaterials_[materialIndex], color))
		{
			meshComponent->GetMeshInstance()->SetMaterial(materialIndex, materialInstance);
		}
	}
	meshComponent->GetMeshInstance()->SetIsCastingShadow(false);

	lineObject->SetWorldPosition(start, false);
	lineObject->SetWorldScaling(Vector3(lineLength, thickness, thickness), false);
	lineObject->SetWorldRotation(Quaternion::FromTwoVectors(Vector3::ForwardVector, lineDirection.GetNormalized()));
	debugVisualObjects_.push_back(lineObject);
}

void NavigationPanel::ApplyTriangleTransform(ObjectBase* object, const Vector3& point0, const Vector3& point1, const Vector3& point2) const
{
	NavigationDebugObject* debugObject = dynamic_cast<NavigationDebugObject*>(object);
	if (!debugObject)
	{
		return;
	}

	const Vector3 edgeX = point1 - point0;
	const Vector3 edgeY = point2 - point0;
	Vector3 normal = edgeX.Cross(edgeY);
	if (normal.SquareLength() <= SMALLER_EPSILON)
	{
		normal = Vector3::UpVector;
	}
	else
	{
		normal.Normalize();
	}

	const Vector3 origin = point0 + normal * kVisualZOffset;
	const Matrix transform(
		edgeX.x, edgeY.x, normal.x, origin.x,
		edgeX.y, edgeY.y, normal.y, origin.y,
		edgeX.z, edgeY.z, normal.z, origin.z,
		0.f, 0.f, 0.f, 1.f);

	debugObject->SetWorldTransformationMatrix(transform);
}

MaterialInstance* NavigationPanel::CreateMaterialInstance(Material* sourceMaterial, const Vector4& baseColor) const
{
	if (!sourceMaterial)
	{
		return nullptr;
	}

	MaterialInstance* materialInstance = MaterialInstance::Create(sourceMaterial);

	materialInstance->SetBaseColor(baseColor);
	materialInstance->SetBlendModel(MaterialBlendModel::Transparent);
	materialInstance->SetShadingModel(MaterialShadingModel::TwoSided);
	materialInstance->SetTranslucency(baseColor.w);
	materialInstance->SetRoughness(0.8f);
	materialInstance->SetMetallic(0.f);

	return materialInstance;
}
