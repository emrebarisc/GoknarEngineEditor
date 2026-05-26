#pragma once

#include <array>
#include <string>
#include <vector>

#include "Goknar/Navigation/NavigationTree.h"
#include "Goknar/Navigation/NavigationTypes.h"
#include "Goknar/Navigation/PathFinder.h"
#include "UI/Panels/EditorPanel.h"

class Material;
class MaterialInstance;
class ObjectBase;
class StaticMesh;

enum class NavigationPointGizmoAxis
{
	None = -1,
	X = 0,
	Y,
	Z
};

class NavigationPanel : public IEditorPanel
{
public:
	NavigationPanel(EditorHUD* hud);
	virtual ~NavigationPanel();

	virtual void Init() override;
	virtual void Draw() override;
	virtual void SetIsOpen(bool isOpen) override;

	bool HandleViewportLeftClick();

private:
	enum class EditorMode
	{
		Select = 0,
		Draw
	};

	std::string GetDefaultNavigationTreePath() const;
	void DrawToolbar();
	void DrawNodeList();
	void DrawSelectedNodeProperties();
	void DrawSettings();
	void DrawPathFinder();
	void DrawPathPointEditor(int pointIndex, const char* label);

	NavigationTree* GetSceneNavigationTree() const;
	NavigationTree* GetActiveNavigationTree();
	const NavigationTree* GetActiveNavigationTree() const;
	bool IsUsingSceneNavigationMesh() const;
	bool CanEditActiveNavigationTree() const;
	void RebuildSceneNavigationMesh(bool updateStatus = false);
	void ValidateSelectedNode();

	void AddNodeAtPosition(const Vector3& position);
	void SelectNode(NavigationNode* node);
	NavigationNode* FindNodeAtPosition(const Vector3& position) const;
	void DeleteSelectedNode();
	void RebuildNeighbours();
	void DivideSelectedNodeIntoGrid();

	bool GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const;
	bool TryGetViewportRaycastHit(Vector3& outPosition) const;
	bool IsCursorInsideViewport() const;
	bool HandlePathPointPickClick();
	void FindAndDrawPath();
	bool IsPathSegmentWalkable(const Vector3& start, const Vector3& end) const;
	void DrawPathDebugLine(const std::vector<NavigationPath>& path);
	void ClearPathDebugDraws();

	void UpdatePointGizmoDrag();
	void EndPointGizmoDrag(bool rebuildNeighbours);
	void ClearPointGizmoSelection();
	bool TryHandlePointGizmoClick(const Vector3& rayOrigin, const Vector3& rayDirection);
	bool FindPointHandleUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, int& outPointIndex) const;
	bool FindAxisUnderRay(const Vector3& rayOrigin, const Vector3& rayDirection, NavigationPointGizmoAxis& outAxis) const;

	void MarkVisualsDirty();
	void RebuildDebugVisuals();
	void ClearDebugVisuals();
	void CreateNodeVisual(NavigationNode* node);
	void CreateNodeTriangleVisual(NavigationNode* node, const Vector3& point0, const Vector3& point1, const Vector3& point2, const Vector4& color);
	void CreateNeighbourVisual(const NavigationNode* firstNode, const NavigationNode* secondNode);
	void CreateSelectedNodePointVisuals();
	void CreatePointHandleVisual(int pointIndex, bool isSelected);
	void CreateAxisArrowVisual(NavigationPointGizmoAxis axis);
	void CreateLineVisual(const char* objectName, const Vector3& start, const Vector3& end, const Vector4& color, float thickness);
	void ApplyTriangleTransform(ObjectBase* object, const Vector3& point0, const Vector3& point1, const Vector3& point2) const;
	MaterialInstance* CreateMaterialInstance(Material* sourceMaterial, const Vector4& baseColor) const;

	NavigationTree* navigationTree_{ new NavigationTree() };
	NavigationNode* selectedNode_{ nullptr };

	NavMeshSettings navMeshSettings_{};
	EditorMode editorMode_{ EditorMode::Select };
	bool useSceneNavigationMesh_{ true };

	std::string navigationTreePath_{};
	std::array<char, 512> navigationTreePathBuffer_{};
	std::string statusText_{};

	float defaultNodeHalfExtent_{ 1.f };
	float neighbourEdgeConnectionDistance_{ 0.1f };
	float visualLineThickness_{ 0.04f };
	int gridDivisionCountX_{ 2 };
	int gridDivisionCountY_{ 2 };

	std::array<Vector3, 2> pathPoints_{};
	std::array<bool, 2> hasPathPoint_{};
	int pathPointPickIndex_{ -1 };
	ObjectBase* pathDebugOwner_{ nullptr };

	int selectedPointIndex_{ -1 };
	NavigationPointGizmoAxis selectedGizmoAxis_{ NavigationPointGizmoAxis::None };
	bool isDraggingPoint_{ false };
	NavigationNode* draggedNode_{ nullptr };
	Vector3 dragStartPoint_{ Vector3::ZeroVector };
	Vector3 dragStartGizmoOrigin_{ Vector3::ZeroVector };
	float dragStartAxisParameter_{ 0.f };

	StaticMesh* triangleMesh_{ nullptr };
	StaticMesh* lineMesh_{ nullptr };
	StaticMesh* cornerSphereMesh_{ nullptr };
	StaticMesh* gizmoArrowMesh_{ nullptr };
	std::vector<Material*> triangleSourceMaterials_;
	std::vector<Material*> lineSourceMaterials_;
	std::vector<Material*> cornerSphereSourceMaterials_;
	std::vector<Material*> gizmoArrowSourceMaterials_;
	std::vector<ObjectBase*> debugVisualObjects_;
	bool areVisualsDirty_{ true };
};
