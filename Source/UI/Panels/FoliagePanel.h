#pragma once

#include "EditorPanel.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Math/Matrix.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class InstancedStaticMeshComponent;
class ObjectBase;
class Scene;
class StaticMesh;
class StaticMeshComponent;

struct FoliageCellCoord
{
	int x{ 0 };
	int y{ 0 };
	int z{ 0 };

	bool operator==(const FoliageCellCoord& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

struct FoliageCellCoordHasher
{
	std::size_t operator()(const FoliageCellCoord& coord) const
	{
		const std::size_t xHash = std::hash<int>{}(coord.x);
		const std::size_t yHash = std::hash<int>{}(coord.y);
		const std::size_t zHash = std::hash<int>{}(coord.z);
		return xHash ^ (yHash << 1) ^ (zHash << 2);
	}
};

class FoliagePanel : public IEditorPanel
{
public:
	FoliagePanel(EditorHUD* hud);
	virtual ~FoliagePanel();

	virtual void Init() override;
	virtual void Draw() override;
	virtual void SetIsOpen(bool isOpen) override;

	bool HandleViewportLeftPressed();
	void HandleViewportLeftReleased();
	void PrepareSceneForSave();

private:
	struct MeshEntry
	{
		std::string meshPath;
		StaticMesh* mesh{ nullptr };
		bool enabled{ true };
		float spawnWeight{ 1.f };
		float densityMultiplier{ 1.f };
		float minScale{ 1.f };
		float maxScale{ 1.f };
		bool randomYaw{ true };
		bool alignToSurfaceNormal{ true };
		float groundOffset{ 0.f };
	};

	struct PlacedInstance
	{
		std::string meshPath;
		Matrix worldTransform{ Matrix::IdentityMatrix };
		Vector3 worldPosition{ Vector3::ZeroVector };
	};

	struct BrushHit
	{
		Vector3 position{ Vector3::ZeroVector };
		Vector3 normal{ Vector3::UpVector };
	};

	struct CellRuntime
	{
		ObjectBase* object{ nullptr };
		std::unordered_map<std::string, InstancedStaticMeshComponent*> componentsByMeshPath;
	};

	using CellSet = std::unordered_set<FoliageCellCoord, FoliageCellCoordHasher>;
	using CellRuntimeMap = std::unordered_map<FoliageCellCoord, CellRuntime, FoliageCellCoordHasher>;

	void DrawBrushSettings();
	void DrawMeshList();
	void DrawHistoryControls();
	void DrawNotes();
	void OpenMeshSelector();
	void OnMeshAssetSelected(const std::string& path);

	bool IsCursorInsideViewport() const;
	bool GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const;
	bool Raycast(const Vector3& from, const Vector3& to, BrushHit& outHit) const;
	bool TryGetBrushHit(BrushHit& outHit) const;

	void UpdateToolState();
	void SetToolActive(bool active);
	void EnsureBrushPreview();
	void UpdateBrushPreview();
	void HideBrushPreview();
	void DestroyBrushPreview();

	void BeginStroke();
	void EndStroke();
	void ApplyBrushAtCursor(bool force);
	bool PaintAtHit(const BrushHit& centerHit, CellSet& outAffectedCells);
	bool EraseAtHit(const BrushHit& centerHit, CellSet& outAffectedCells);
	MeshEntry* ChooseMeshEntry();
	bool IsFarEnoughFromExistingInstances(const Vector3& position) const;

	void SynchronizeFromScene();
	void ClearRuntimeCells(bool destroySceneObjects);
	void RebuildAllCells();
	void RebuildCells(const CellSet& cells);
	void RebuildCell(const FoliageCellCoord& coord);
	void RemoveRuntimeCellObject(const FoliageCellCoord& coord);
	CellRuntime& GetOrCreateRuntimeCell(const FoliageCellCoord& coord);
	InstancedStaticMeshComponent* GetOrCreateInstancedComponent(
		CellRuntime& runtime,
		const FoliageCellCoord& coord,
		const std::string& meshPath);

	FoliageCellCoord GetCellCoord(const Vector3& position) const;
	Vector3 GetCellOrigin(const FoliageCellCoord& coord) const;
	std::string GetCellObjectName(const FoliageCellCoord& coord) const;
	std::string NormalizeMeshPath(const std::string& path) const;
	StaticMesh* ResolveMesh(const std::string& meshPath) const;
	std::size_t GetInstanceCount() const;
	Scene* GetCurrentScene() const;

	void PushUndoSnapshot();
	void ApplySnapshot(const std::vector<PlacedInstance>& snapshot);
	void UndoStroke();
	void RedoStroke();

	std::vector<MeshEntry> meshEntries_;
	std::vector<PlacedInstance> instances_;
	CellRuntimeMap runtimeCells_;

	std::vector<std::vector<PlacedInstance>> undoStack_;
	std::vector<std::vector<PlacedInstance>> redoStack_;
	std::vector<PlacedInstance> strokeStartSnapshot_;

	ObjectBase* brushPreviewObject_{ nullptr };
	StaticMeshComponent* brushPreviewComponent_{ nullptr };

	Scene* synchronizedScene_{ nullptr };
	bool hasSynchronizedScene_{ false };
	bool isToolActive_{ false };
	bool eraseMode_{ false };
	bool isStrokeInProgress_{ false };
	bool strokeChangedInstances_{ false };
	bool hasLastStrokePoint_{ false };
	Vector3 lastStrokePoint_{ Vector3::ZeroVector };

	float brushRadius_{ 3.f };
	float density_{ 0.35f };
	float gridSize_{ 100.f };
	float minimumSpacing_{ 0.f };
	int maxAttemptsPerBrushStep_{ 256 };
	std::uint32_t randomSeed_{ 1337u };
	std::mt19937 randomGenerator_;
};
