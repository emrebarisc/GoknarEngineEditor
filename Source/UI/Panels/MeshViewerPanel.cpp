#include "MeshViewerPanel.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"

namespace
{
	constexpr unsigned int MeshViewerRenderMask = 0x10000000;

	bool HasStaticMeshSelection(const StaticMesh* staticMesh)
	{
		return staticMesh && !staticMesh->GetSubMeshes().empty();
	}

	bool IsStaticMeshReadyForPreview(const StaticMesh* staticMesh)
	{
		if (!HasStaticMeshSelection(staticMesh))
		{
			return false;
		}

		for (const MeshUnit* subMesh : staticMesh->GetSubMeshes())
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

void MeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMesh)
{
	ClearPreviewMaterialOverrides();
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
	targetStaticMesh_ = nullptr;

	if (!staticMesh)
	{
		staticMeshComponent_->SetIsActive(false);
		OnTargetMeshChanged();
		return;
	}

	targetStaticMesh_ = staticMesh;

	if (IsStaticMeshReadyForPreview(targetStaticMesh_))
	{
		staticMeshComponent_->SetMesh(staticMesh);
		staticMeshComponent_->SetIsActive(true);
		staticMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
		staticMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
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
	return targetStaticMesh_ ? targetStaticMesh_->GetPath() : "";
}

const Box* MeshViewerPanel::GetCurrentMeshBounds() const
{
	return targetStaticMesh_ ? &targetStaticMesh_->GetAABB() : nullptr;
}

size_t MeshViewerPanel::GetSubMeshCount() const
{
	return targetStaticMesh_ ? targetStaticMesh_->GetSubMeshes().size() : 0;
}

std::string MeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

size_t MeshViewerPanel::GetSubMeshVertexCount(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetVertexCount() : 0;
}

size_t MeshViewerPanel::GetSubMeshFaceCount(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetFaceCount() : 0;
}

bool MeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	if (!HasCurrentMesh() || !subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	return RebuildMaterialForSubMesh(subMesh, materialPath);
}

MaterialInstance* MeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
	if (!IsCurrentMeshReadyToView())
	{
		return nullptr;
	}

	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
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
	if (!meshInstance || subMeshIndex >= targetStaticMesh_->GetSubMeshes().size())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	meshInstance->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);
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

	for (MeshUnit* subMesh : targetStaticMesh_->GetSubMeshes())
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
	StaticMesh* currentMesh = meshInstance ? meshInstance->GetMesh() : nullptr;
	if (!meshInstance || !currentMesh)
	{
		return;
	}

	const size_t subMeshCount = currentMesh->GetSubMeshes().size();
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
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

MeshUnit* MeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetStaticMesh_ || subMeshIndex >= targetStaticMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetStaticMesh_->GetSubMeshes()[subMeshIndex];
}

Material* MeshViewerPanel::GetPreviewDefaultMaterial(MeshUnit* subMesh) const
{
	if (!previewDefaultMaterial_)
	{
		previewDefaultMaterial_ = CreateInitializedPreviewDefaultMaterial(subMesh, "__Editor__MeshViewerDefaultMaterial");
	}

	return previewDefaultMaterial_;
}

Material* MeshViewerPanel::GetMaterialSlotVisualizerMaterial(MeshUnit* subMesh) const
{
	if (!materialSlotVisualizerMaterial_)
	{
		materialSlotVisualizerMaterial_ = CreateInitializedMaterialSlotVisualizerMaterial(subMesh, "__Editor__MeshViewerMaterialSlotVisualizer");
	}

	return materialSlotVisualizerMaterial_;
}
