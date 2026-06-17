#pragma once

#include "MeshAssetViewerPanelBase.h"

class MaterialInstance;
class MeshUnit;
class StaticMesh;
class StaticMeshComponent;

class MeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	explicit MeshViewerPanel(EditorHUD* hud);
	~MeshViewerPanel() override;

	void SetTargetStaticMesh(StaticMesh* staticMesh);

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

	void ClearPreviewMaterialOverrides();
	MeshUnit* GetSubMesh(size_t subMeshIndex) const;

	StaticMeshComponent* staticMeshComponent_{ nullptr };
	StaticMesh* targetStaticMesh_{ nullptr };
};
