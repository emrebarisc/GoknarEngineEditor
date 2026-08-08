#pragma once

#include "MeshAssetViewerPanelBase.h"

class MaterialInstance;
class SkeletalMesh;
class SkeletalMeshComponent;
class SkeletalMeshUnit;

class SkeletalMeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	explicit SkeletalMeshViewerPanel(EditorHUD* hud);
	~SkeletalMeshViewerPanel() override;

	void SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh);

private:
	bool HasCurrentMesh() const override;
	bool IsCurrentMeshReadyToView() const override;
	std::string GetCurrentMeshPath() const override;
	const Box* GetCurrentMeshBounds() const override;
	size_t GetSubMeshCount() const override;
	std::string GetSubMeshName(size_t subMeshIndex) const override;
	size_t GetSubMeshVertexCount(size_t subMeshIndex) const override;
	size_t GetSubMeshFaceCount(size_t subMeshIndex) const override;
	bool RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath) override;
	MaterialInstance* CreatePreviewMaterialInstance(size_t subMeshIndex) const override;
	void SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance) override;
	const char* GetNoMeshSelectedText() const override;
	const char* GetMeshNotReadyText() const override;
	void InitializeCurrentMeshMaterials() override;
	bool HasAdditionalSidePanelContent() const override;
	void DrawAdditionalSidePanelContent() override;

	void ClearPreviewMaterialOverrides();
	void ClearPreviewDefaultMaterial();
	void ClearMaterialSlotVisualizerMaterial();
	SkeletalMeshUnit* GetSubMesh(size_t subMeshIndex) const;
	Material* GetPreviewDefaultMaterial(SkeletalMeshUnit* subMesh) const;
	Material* GetMaterialSlotVisualizerMaterial(SkeletalMeshUnit* subMesh) const;

	SkeletalMeshComponent* skeletalMeshComponent_{ nullptr };
	SkeletalMesh* targetSkeletalMesh_{ nullptr };
	mutable Material* previewDefaultMaterial_{ nullptr };
	mutable Material* materialSlotVisualizerMaterial_{ nullptr };
};
