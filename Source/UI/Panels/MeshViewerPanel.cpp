#include "MeshViewerPanel.h"

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"

namespace
{
	constexpr unsigned int MeshViewerRenderMask = 0x10000000;
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
}

void MeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMesh)
{
	ClearPreviewMaterialOverrides();
	targetStaticMesh_ = nullptr;

	if (!staticMesh)
	{
		staticMeshComponent_->SetIsActive(false);
		OnTargetMeshChanged();
		return;
	}

	staticMeshComponent_->SetMesh(staticMesh);
	staticMeshComponent_->SetIsActive(true);
	staticMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
	staticMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
	targetStaticMesh_ = staticMesh;

	OnTargetMeshChanged();
}

bool MeshViewerPanel::HasCurrentMesh() const
{
	return targetStaticMesh_ && !targetStaticMesh_->GetSubMeshes().empty();
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
	if (!subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	Material* material = subMesh->GetMaterial();
	if (!material)
	{
		return false;
	}

	material->ResetForRebuild();
	MaterialSerializer::Deserialize(materialPath, material);
	material->Build(subMesh);

	if (subMesh->GetIsInitialized())
	{
		material->PreInit();
		material->Init();
		material->PostInit();
	}

	return true;
}

MaterialInstance* MeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
	MeshUnit* subMesh = GetSubMesh(subMeshIndex);
	Material* material = subMesh ? subMesh->GetMaterial() : nullptr;
	return material ? MaterialInstance::Create(material) : nullptr;
}

void MeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
	if (!staticMeshComponent_ || !targetStaticMesh_)
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

MeshUnit* MeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetStaticMesh_ || subMeshIndex >= targetStaticMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetStaticMesh_->GetSubMeshes()[subMeshIndex];
}
