#pragma once

#include "MeshAssetViewerPanelBase.h"

class MaterialInstance;
class MeshUnit;
class StaticMesh;
class StaticMeshComponent;

class StaticMeshViewerPanel : public MeshAssetViewerPanelBase
{
public:
	explicit StaticMeshViewerPanel(EditorHUD* hud);
	~StaticMeshViewerPanel() override;

	void SetTargetStaticMesh(StaticMesh* staticMesh);

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
	void RefreshPreviewRenderData();

	void ClearPreviewMaterialOverrides();
	void ClearPreviewDefaultMaterial();
	MeshUnit* GetSubMesh(size_t subMeshIndex) const;
	Material* GetPreviewDefaultMaterial(MeshUnit* subMesh) const;

	StaticMeshComponent* staticMeshComponent_{ nullptr };
	StaticMesh* targetStaticMesh_{ nullptr };
	mutable Material* previewDefaultMaterial_{ nullptr };
};
