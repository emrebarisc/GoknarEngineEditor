#pragma once

#include "MeshAssetViewerPanelBase.h"

class SkeletalMeshComponent;
class SkeletalMesh;
class MaterialInstance;

class SkeletalMeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	SkeletalMeshViewerPanel(EditorHUD* hud);
	virtual ~SkeletalMeshViewerPanel();

	void SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh);

private:
	virtual bool HasCurrentMesh() const override;
	virtual std::string GetCurrentMeshPath() const override;
	virtual const Box* GetCurrentMeshBounds() const override;
	virtual size_t GetSubMeshCount() const override;
	virtual std::string GetSubMeshName(size_t subMeshIndex) const override;
	virtual bool RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath) override;
	virtual MaterialInstance* CreatePreviewMaterialInstance(size_t subMeshIndex) const override;
	virtual void SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance) override;
	virtual const char* GetNoMeshSelectedText() const override;
	virtual bool HasAdditionalSidePanelContent() const override;
	virtual void DrawAdditionalSidePanelContent() override;
	virtual float GetStackedSidePanelHeight() const override;

	SkeletalMesh* GetCurrentSkeletalMesh() const;

	SkeletalMeshComponent* skeletalMeshComponent_{ nullptr };
};
