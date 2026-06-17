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
	bool HasAdditionalSidePanelContent() const override;
	void DrawAdditionalSidePanelContent() override;

	void ClearPreviewMaterialOverrides();
	SkeletalMeshUnit* GetSubMesh(size_t subMeshIndex) const;

	SkeletalMeshComponent* skeletalMeshComponent_{ nullptr };
	SkeletalMesh* targetSkeletalMesh_{ nullptr };
};
