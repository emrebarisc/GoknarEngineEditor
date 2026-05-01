#pragma once

#include "MeshAssetViewerPanelBase.h"

class StaticMeshComponent;
class StaticMesh;
class MaterialInstance;

class MeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	MeshViewerPanel(EditorHUD* hud);
	virtual ~MeshViewerPanel();

	void SetTargetStaticMesh(StaticMesh* staticMesh);

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

	StaticMesh* GetCurrentStaticMesh() const;

	StaticMeshComponent* staticMeshComponent_{ nullptr };
};
