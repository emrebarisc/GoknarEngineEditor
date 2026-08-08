#pragma once

#include <string>
#include <vector>

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"

class Box;
class Material;
class MaterialInstance;
class MeshUnit;
class MeshViewerCameraObject;
class ObjectBase;
class RenderTarget;

class MeshAssetViewerPanelBase : public IEditorPanel
{
public:
	MeshAssetViewerPanelBase(
		const std::string& title,
		EditorHUD* hud,
		const std::string& cameraObjectName,
		const std::string& viewedObjectName,
		unsigned int renderMask,
		const std::string& viewportChildId,
		const std::string& sidePanelChildId);
	~MeshAssetViewerPanelBase() override;

	void Init() override;
	void Draw() override;
	void SetIsOpen(bool isOpen) override;

protected:
	void OnTargetMeshChanged();

	ObjectBase* GetViewedObject() const
	{
		return viewedObject_;
	}

	unsigned int GetRenderMask() const
	{
		return renderMask_;
	}

	bool DoesMaterialAssetExist(const std::string& materialPath) const;
	bool HasMaterialAssetOverride(size_t subMeshIndex) const;
	bool IsMaterialSlotVisualizerEnabled() const;
	bool IsMaterialSlotUnset(size_t subMeshIndex) const;
	bool RebuildMaterialForSubMesh(MeshUnit* subMesh, const std::string& materialPath) const;
	void InitializeMaterialForSubMesh(MeshUnit* subMesh) const;
	Vector4 GetMaterialSlotVisualizerColor(size_t subMeshIndex) const;
	Material* CreateInitializedPreviewDefaultMaterial(MeshUnit* subMesh, const char* materialName) const;
	Material* CreateInitializedMaterialSlotVisualizerMaterial(MeshUnit* subMesh, const char* materialName) const;
	MaterialInstance* CreatePreviewDefaultMaterialInstance(Material* material) const;
	MaterialInstance* CreateMaterialSlotVisualizerMaterialInstance(Material* material, size_t subMeshIndex, bool isMaterialUnset) const;
	void DestroyPreviewDefaultMaterial(Material*& material) const;

private:
	void DrawViewport();
	void DrawSidePanel();
	void DrawMeshProperties();
	void DrawMaterialSelector(size_t subMeshIndex);
	void DrawEmptyViewportMessage(const char* message);
	void ResetCameraToCurrentMesh();
	void RefreshPreviewMaterialOverrides();
	void SetPreviewRenderActive(bool active);
	void OnMaterialSelected(const std::string& path);
	bool CanRenderCurrentMesh() const;

	virtual bool HasCurrentMesh() const = 0;
	virtual bool IsCurrentMeshReadyToView() const = 0;
	virtual std::string GetCurrentMeshPath() const = 0;
	virtual const Box* GetCurrentMeshBounds() const = 0;
	virtual size_t GetSubMeshCount() const = 0;
	virtual std::string GetSubMeshName(size_t subMeshIndex) const = 0;
	virtual size_t GetSubMeshVertexCount(size_t subMeshIndex) const = 0;
	virtual size_t GetSubMeshFaceCount(size_t subMeshIndex) const = 0;
	virtual bool RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath) = 0;
	virtual MaterialInstance* CreatePreviewMaterialInstance(size_t subMeshIndex) const = 0;
	virtual void SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance) = 0;
	virtual const char* GetNoMeshSelectedText() const = 0;
	virtual const char* GetMeshNotReadyText() const;
	virtual void InitializeCurrentMeshMaterials();
	virtual bool HasAdditionalSidePanelContent() const;
	virtual void DrawAdditionalSidePanelContent();

	RenderTarget* renderTarget_{ nullptr };
	MeshViewerCameraObject* cameraObject_{ nullptr };
	ObjectBase* viewedObject_{ nullptr };

	Vector2 viewportSize_{ 1024.f, 1024.f };
	std::vector<std::string> selectedMaterialPaths_{};
	int pendingMaterialSelectionSubMeshIndex_{ -1 };
	bool materialSlotVisualizerEnabled_{ false };
	unsigned int renderMask_{ 0 };
	std::string viewportChildId_{};
	std::string sidePanelChildId_{};
};
