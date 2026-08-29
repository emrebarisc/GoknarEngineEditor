#include "MeshViewerPanel.h"

#include "Goknar/Engine.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Model/Mesh.h"
#include "Goknar/Model/MeshGeometry.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"
#include "Goknar/Renderer/Renderer.h"

namespace
{
	constexpr unsigned int MeshViewerRenderMask = 0x10000000;

	bool HasStaticMeshSelection(const StaticMeshLOD* staticMesh)
	{
		return staticMesh && !staticMesh->GetSubMeshes().empty();
	}

	bool IsStaticMeshReadyForPreview(const StaticMeshLOD* staticMesh)
	{
		if (!HasStaticMeshSelection(staticMesh))
		{
			return false;
		}

		for (const MeshGeometry* subMesh : staticMesh->GetSubMeshes())
		{
			if (!subMesh || subMesh->GetVertexCount() == 0 || subMesh->GetFaceCount() == 0)
			{
				return false;
			}
		}

		return true;
	}
}

MeshViewerPanel::MeshViewerPanel(EditorHUD* hud) :
	MeshAssetViewerPanelBase(
		"Mesh Viewer",
		hud,
		"__Editor__StaticMeshViewerCamera",
		"__Editor__StaticMeshViewerTarget",
		MeshViewerRenderMask,
		"StaticMeshViewerViewport",
		"StaticMeshViewerProperties")
{
	staticMeshComponent_ = GetViewedObject()->AddSubComponent<StaticMeshComponent>();
	staticMeshComponent_->SetIsActive(false);
	staticMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
	staticMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
}

MeshViewerPanel::~MeshViewerPanel()
{
	ClearPreviewMaterialOverrides();
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
}

void MeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMeshContainer)
{
	ClearPreviewMaterialOverrides();
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
	targetStaticMeshContainer_ = nullptr;
	targetStaticMesh_ = nullptr;

	if (!staticMeshContainer)
	{
		staticMeshComponent_->SetIsActive(false);
		OnTargetMeshChanged();
		return;
	}

	targetStaticMeshContainer_ = staticMeshContainer;
	targetStaticMesh_ = staticMeshContainer->GetLOD(0);

	if (IsStaticMeshReadyForPreview(targetStaticMesh_))
	{
		staticMeshComponent_->SetMesh(staticMeshContainer);
		staticMeshComponent_->SetIsActive(true);
		staticMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
		staticMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
		staticMeshComponent_->GetMeshInstance()->SetForcedLODIndex(-1);
	}
	else
	{
		staticMeshComponent_->SetIsActive(false);
	}

	OnTargetMeshChanged();
}

bool MeshViewerPanel::HasCurrentMesh() const
{
	return HasStaticMeshSelection(targetStaticMesh_);
}

bool MeshViewerPanel::IsCurrentMeshReadyToView() const
{
	return IsStaticMeshReadyForPreview(targetStaticMesh_);
}

std::string MeshViewerPanel::GetCurrentMeshPath() const
{
	return targetStaticMeshContainer_ ? targetStaticMeshContainer_->GetPath() : "";
}

const Box* MeshViewerPanel::GetCurrentMeshBounds() const
{
	return targetStaticMesh_ ? &targetStaticMesh_->GetAABB() : nullptr;
}

const Box* MeshViewerPanel::GetCurrentMeshCoverageBounds() const
{
	return targetStaticMeshContainer_ ? &targetStaticMeshContainer_->GetAABB() : nullptr;
}

const Matrix* MeshViewerPanel::GetCurrentMeshWorldTransformationMatrix() const
{
	return staticMeshComponent_ ? &staticMeshComponent_->GetComponentToWorldTransformationMatrix() : nullptr;
}

size_t MeshViewerPanel::GetLODCount() const
{
	return targetStaticMeshContainer_ ? targetStaticMeshContainer_->GetLODCount() : 0;
}

size_t MeshViewerPanel::GetLODIndexForFrameCoverage(float frameCoverage) const
{
	return targetStaticMeshContainer_ ? targetStaticMeshContainer_->GetLODIndex(frameCoverage) : 0;
}

float MeshViewerPanel::GetLODFrameCoverage(size_t LODIndex) const
{
	return targetStaticMeshContainer_ ? targetStaticMeshContainer_->GetLODFrameCoverage((int)LODIndex) : 0.f;
}

void MeshViewerPanel::SetLODFrameCoverage(size_t LODIndex, float frameCoverage)
{
	if (targetStaticMeshContainer_)
	{
		targetStaticMeshContainer_->SetLODFrameCoverage((int)LODIndex, frameCoverage);
	}
}

bool MeshViewerPanel::SetCurrentLODIndex(size_t LODIndex)
{
	if (!targetStaticMeshContainer_)
	{
		targetStaticMesh_ = nullptr;
		return false;
	}

	ClearPreviewMaterialOverrides();
	targetStaticMesh_ = targetStaticMeshContainer_->GetLOD((int)LODIndex);

	StaticMeshInstance* meshInstance = staticMeshComponent_ ? staticMeshComponent_->GetMeshInstance() : nullptr;
	if (meshInstance)
	{
		meshInstance->SetForcedLODIndex(IsLODSelectionAutomatic() ? -1 : static_cast<int>(LODIndex));
	}

	return targetStaticMesh_ != nullptr;
}

size_t MeshViewerPanel::GetSubMeshCount() const
{
	return targetStaticMesh_ ? targetStaticMesh_->GetSubMeshes().size() : 0;
}

std::string MeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
	MeshGeometry* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

size_t MeshViewerPanel::GetSubMeshVertexCount(size_t subMeshIndex) const
{
	MeshGeometry* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetVertexCount() : 0;
}

size_t MeshViewerPanel::GetSubMeshFaceCount(size_t subMeshIndex) const
{
	MeshGeometry* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetFaceCount() : 0;
}

size_t MeshViewerPanel::GetLODSubMeshCount(size_t LODIndex) const
{
	if (!targetStaticMeshContainer_ || GetLODCount() <= LODIndex)
	{
		return 0;
	}

	StaticMeshLOD* LODMesh = targetStaticMeshContainer_->GetLOD((int)LODIndex);
	return LODMesh ? LODMesh->GetSubMeshes().size() : 0;
}

std::string MeshViewerPanel::GetLODSubMeshName(size_t LODIndex, size_t subMeshIndex) const
{
	MeshGeometry* subMesh = GetLODSubMesh(LODIndex, subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

bool MeshViewerPanel::RebuildMaterial(size_t LODIndex, size_t subMeshIndex, const std::string& materialPath)
{
	MeshGeometry* subMesh = GetLODSubMesh(LODIndex, subMeshIndex);
	if (!targetStaticMeshContainer_ || !subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	return RebuildMaterialForSubMesh(subMesh, materialPath);
}

bool MeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	return RebuildMaterial(GetSelectedLODIndex(), subMeshIndex, materialPath);
}

MaterialInstance* MeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
	if (!IsCurrentMeshReadyToView())
	{
		return nullptr;
	}

	MeshGeometry* subMesh = GetSubMesh(subMeshIndex);
	Material* material = subMesh ? subMesh->GetMaterial() : nullptr;
	if (IsMaterialSlotVisualizerEnabled())
	{
		return CreateMaterialSlotVisualizerMaterialInstance(
			GetMaterialSlotVisualizerMaterial(subMesh),
			subMeshIndex,
			IsMaterialSlotUnset(subMeshIndex));
	}

	if (!HasMaterialAssetOverride(subMeshIndex))
	{
		return CreatePreviewDefaultMaterialInstance(GetPreviewDefaultMaterial(subMesh));
	}

	return material ? MaterialInstance::Create(material) : CreatePreviewDefaultMaterialInstance(GetPreviewDefaultMaterial(subMesh));
}

void MeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
	if (!staticMeshComponent_ || !targetStaticMesh_ || !IsCurrentMeshReadyToView())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	StaticMeshInstance* meshInstance = staticMeshComponent_->GetMeshInstance();
	if (!meshInstance ||
		subMeshIndex >= targetStaticMesh_->GetSubMeshes().size() ||
		subMeshIndex >= meshInstance->GetMaterials().size())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	meshInstance->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);
}

void MeshViewerPanel::RefreshPreviewRenderData()
{
	if (!staticMeshComponent_ || !targetStaticMesh_ || !IsCurrentMeshReadyToView())
	{
		return;
	}

	StaticMeshInstance* meshInstance = staticMeshComponent_->GetMeshInstance();
	if (!meshInstance || !meshInstance->GetMesh())
	{
		return;
	}

	engine->GetRenderer()->RemoveStaticMeshInstance(meshInstance);
	engine->GetRenderer()->AddStaticMeshInstance(meshInstance);
}

const char* MeshViewerPanel::GetNoMeshSelectedText() const
{
	return "No static mesh selected.";
}

const char* MeshViewerPanel::GetMeshNotReadyText() const
{
	return "Static mesh is not ready to view. It has not been sent to the GPU yet.";
}

void MeshViewerPanel::InitializeCurrentMeshMaterials()
{
	if (!targetStaticMesh_)
	{
		return;
	}

	for (MeshGeometry* subMesh : targetStaticMesh_->GetSubMeshes())
	{
		InitializeMaterialForSubMesh(subMesh);
	}
}

void MeshViewerPanel::ClearPreviewMaterialOverrides()
{
	if (!staticMeshComponent_)
	{
		return;
	}

	StaticMeshInstance* meshInstance = staticMeshComponent_->GetMeshInstance();
	StaticMesh* currentMeshContainer = meshInstance ? meshInstance->GetMesh() : nullptr;
	StaticMeshLOD* currentMesh = currentMeshContainer ? currentMeshContainer->GetLOD(0) : nullptr;
	if (!meshInstance || !currentMesh)
	{
		return;
	}

	for (size_t subMeshIndex = 0; subMeshIndex < meshInstance->GetMaterials().size(); ++subMeshIndex)
	{
		meshInstance->SetMaterial(static_cast<int>(subMeshIndex), nullptr);
	}
}

void MeshViewerPanel::ClearPreviewDefaultMaterial()
{
	DestroyPreviewDefaultMaterial(previewDefaultMaterial_);
}

void MeshViewerPanel::ClearMaterialSlotVisualizerMaterial()
{
	DestroyPreviewDefaultMaterial(materialSlotVisualizerMaterial_);
}

MeshGeometry* MeshViewerPanel::GetLODSubMesh(size_t LODIndex, size_t subMeshIndex) const
{
	if (!targetStaticMeshContainer_ || GetLODCount() <= LODIndex)
	{
		return nullptr;
	}

	StaticMeshLOD* LODMesh = targetStaticMeshContainer_->GetLOD((int)LODIndex);
	if (!LODMesh || subMeshIndex >= LODMesh->GetSubMeshes().size())
	{
		return nullptr;
	}

	return LODMesh->GetSubMeshes()[subMeshIndex];
}

MeshGeometry* MeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetStaticMesh_ || subMeshIndex >= targetStaticMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetStaticMesh_->GetSubMeshes()[subMeshIndex];
}

Material* MeshViewerPanel::GetPreviewDefaultMaterial(MeshGeometry* subMesh) const
{
	if (!previewDefaultMaterial_)
	{
		previewDefaultMaterial_ = CreateInitializedPreviewDefaultMaterial(subMesh, "__Editor__MeshViewerDefaultMaterial");
	}

	return previewDefaultMaterial_;
}

Material* MeshViewerPanel::GetMaterialSlotVisualizerMaterial(MeshGeometry* subMesh) const
{
	if (!materialSlotVisualizerMaterial_)
	{
		materialSlotVisualizerMaterial_ = CreateInitializedMaterialSlotVisualizerMaterial(subMesh, "__Editor__MeshViewerMaterialSlotVisualizer");
	}

	return materialSlotVisualizerMaterial_;
}
