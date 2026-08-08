#include "StaticMeshViewerPanel.h"

#include "Goknar/Engine.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"
#include "Goknar/Renderer/Renderer.h"

namespace
{
	constexpr unsigned int StaticMeshViewerRenderMask = 0x10000000;

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

StaticMeshViewerPanel::StaticMeshViewerPanel(EditorHUD* hud) :
	MeshAssetViewerPanelBase(
		"Static Mesh Viewer",
		hud,
		"__Editor__StaticMeshViewerCamera",
		"__Editor__StaticMeshViewerTarget",
		StaticMeshViewerRenderMask,
		"StaticMeshViewerViewport",
		"StaticMeshViewerProperties")
{
	staticMeshComponent_ = GetViewedObject()->AddSubComponent<StaticMeshComponent>();
	staticMeshComponent_->SetIsActive(false);
	staticMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
	staticMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
}

StaticMeshViewerPanel::~StaticMeshViewerPanel()
{
	ClearPreviewMaterialOverrides();
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
}

void StaticMeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMesh)
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

bool StaticMeshViewerPanel::HasCurrentMesh() const
{
	return HasStaticMeshSelection(targetStaticMesh_);
}

bool StaticMeshViewerPanel::IsCurrentMeshReadyToView() const
{
	return IsStaticMeshReadyForPreview(targetStaticMesh_);
}

std::string StaticMeshViewerPanel::GetCurrentMeshPath() const
{
	return targetStaticMesh_ ? targetStaticMesh_->GetPath() : "";
}

const Box* StaticMeshViewerPanel::GetCurrentMeshBounds() const
{
	return targetStaticMesh_ ? &targetStaticMesh_->GetAABB() : nullptr;
}

size_t StaticMeshViewerPanel::GetSubMeshCount() const
{
	return targetStaticMesh_ ? targetStaticMesh_->GetSubMeshes().size() : 0;
}

std::string StaticMeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

size_t StaticMeshViewerPanel::GetSubMeshVertexCount(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetVertexCount() : 0;
}

size_t StaticMeshViewerPanel::GetSubMeshFaceCount(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetFaceCount() : 0;
}

bool StaticMeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	if (!HasCurrentMesh() || !subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	return RebuildMaterialForSubMesh(subMesh, materialPath);
}

MaterialInstance* StaticMeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
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

void StaticMeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
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

const char* StaticMeshViewerPanel::GetNoMeshSelectedText() const
{
	return "No static mesh selected.";
}

const char* StaticMeshViewerPanel::GetMeshNotReadyText() const
{
	return "Static mesh is not ready to view. It has not been sent to the GPU yet.";
}

void StaticMeshViewerPanel::InitializeCurrentMeshMaterials()
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

void StaticMeshViewerPanel::RefreshPreviewRenderData()
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

void StaticMeshViewerPanel::ClearPreviewMaterialOverrides()
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

void StaticMeshViewerPanel::ClearPreviewDefaultMaterial()
{
	DestroyPreviewDefaultMaterial(previewDefaultMaterial_);
}

void StaticMeshViewerPanel::ClearMaterialSlotVisualizerMaterial()
{
	DestroyPreviewDefaultMaterial(materialSlotVisualizerMaterial_);
}

MeshUnit* StaticMeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetStaticMesh_ || subMeshIndex >= targetStaticMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetStaticMesh_->GetSubMeshes()[subMeshIndex];
}

Material* StaticMeshViewerPanel::GetPreviewDefaultMaterial(MeshUnit* subMesh) const
{
	if (!previewDefaultMaterial_)
	{
		previewDefaultMaterial_ = CreateInitializedPreviewDefaultMaterial(subMesh, "__Editor__StaticMeshViewerDefaultMaterial");
	}

	return previewDefaultMaterial_;
}

Material* StaticMeshViewerPanel::GetMaterialSlotVisualizerMaterial(MeshUnit* subMesh) const
{
	if (!materialSlotVisualizerMaterial_)
	{
		materialSlotVisualizerMaterial_ = CreateInitializedMaterialSlotVisualizerMaterial(subMesh, "__Editor__StaticMeshViewerMaterialSlotVisualizer");
	}

	return materialSlotVisualizerMaterial_;
}
