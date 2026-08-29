#pragma once

#include "MeshAssetViewerPanelBase.h"

#include "Goknar/Model/Mesh.h"

class MaterialInstance;
class SkeletalMeshComponent;
class SkeletalMeshGeometry;

class SkeletalMeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	explicit SkeletalMeshViewerPanel(EditorHUD* hud);
	~SkeletalMeshViewerPanel() override;

	void SetTargetSkeletalMesh(SkeletalMesh* skeletalMeshContainer);

private:
	bool HasCurrentMesh() const override;
	bool IsCurrentMeshReadyToView() const override;
	std::string GetCurrentMeshPath() const override;
	const Box* GetCurrentMeshBounds() const override;
	const Box* GetCurrentMeshCoverageBounds() const override;
	const Matrix* GetCurrentMeshWorldTransformationMatrix() const override;
	size_t GetLODCount() const override;
	size_t GetLODIndexForFrameCoverage(float frameCoverage) const override;
	float GetLODFrameCoverage(size_t LODIndex) const override;
	void SetLODFrameCoverage(size_t LODIndex, float frameCoverage) override;
	bool SetCurrentLODIndex(size_t LODIndex) override;
	size_t GetSubMeshCount() const override;
	std::string GetSubMeshName(size_t subMeshIndex) const override;
	size_t GetSubMeshVertexCount(size_t subMeshIndex) const override;
	size_t GetSubMeshFaceCount(size_t subMeshIndex) const override;
	size_t GetLODSubMeshCount(size_t LODIndex) const override;
	std::string GetLODSubMeshName(size_t LODIndex, size_t subMeshIndex) const override;
	bool RebuildMaterial(size_t LODIndex, size_t subMeshIndex, const std::string& materialPath) override;
	bool RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath) override;
	MaterialInstance* CreatePreviewMaterialInstance(size_t subMeshIndex) const override;
	void SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance) override;
	void RefreshPreviewRenderData() override;
	const char* GetNoMeshSelectedText() const override;
	const char* GetMeshNotReadyText() const override;
	void InitializeCurrentMeshMaterials() override;
	bool HasAdditionalSidePanelContent() const override;
	void DrawAdditionalSidePanelContent() override;

	void ClearPreviewMaterialOverrides();
	void ClearPreviewDefaultMaterial();
	void ClearMaterialSlotVisualizerMaterial();
	void CapturePreviewSourceMaterialsIfNeeded();
	void RestorePreviewSourceMaterials();
	SkeletalMeshGeometry* GetLODSubMesh(size_t LODIndex, size_t subMeshIndex) const;
	SkeletalMeshGeometry* GetSubMesh(size_t subMeshIndex) const;
	Material* GetPreviewSourceMaterial(size_t subMeshIndex) const;
	Material* GetPreviewDefaultMaterial(SkeletalMeshGeometry* subMesh) const;
	Material* GetMaterialSlotVisualizerMaterial(SkeletalMeshGeometry* subMesh) const;

	SkeletalMeshComponent* skeletalMeshComponent_{ nullptr };
	SkeletalMesh* targetSkeletalMeshContainer_{ nullptr };
	SkeletalMeshLOD* targetSkeletalMesh_{ nullptr };
	mutable Material* previewDefaultMaterial_{ nullptr };
	mutable Material* materialSlotVisualizerMaterial_{ nullptr };
	std::vector<Material*> previewSourceMaterials_{};
	bool previewMaterialsApplied_{ false };
};
