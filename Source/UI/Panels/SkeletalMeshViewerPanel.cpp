#include "SkeletalMeshViewerPanel.h"

#include "imgui.h"

#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/SkeletalMeshInstance.h"

namespace
{
	constexpr unsigned int SkeletalMeshViewerRenderMask = 0x40000000;
}

SkeletalMeshViewerPanel::SkeletalMeshViewerPanel(EditorHUD* hud) :
	MeshAssetViewerPanelBase(
		"Skeletal Mesh Viewer",
		hud,
		"__Editor__SkeletalMeshViewerCamera",
		"__Editor__SkeletalMeshViewerTarget",
		SkeletalMeshViewerRenderMask,
		"SkeletalMeshViewerViewport",
		"SkeletalMeshViewerProperties")
{
	skeletalMeshComponent_ = GetViewedObject()->AddSubComponent<SkeletalMeshComponent>();
	skeletalMeshComponent_->SetIsActive(false);
	skeletalMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
	skeletalMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
}

SkeletalMeshViewerPanel::~SkeletalMeshViewerPanel()
{
	ClearPreviewMaterialOverrides();
}

void SkeletalMeshViewerPanel::SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh)
{
	ClearPreviewMaterialOverrides();
	targetSkeletalMesh_ = nullptr;

	if (!skeletalMesh)
	{
		skeletalMeshComponent_->SetIsActive(false);
		OnTargetMeshChanged();
		return;
	}

	skeletalMeshComponent_->SetMesh(skeletalMesh);
	skeletalMeshComponent_->SetIsActive(true);
	skeletalMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
	skeletalMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
	targetSkeletalMesh_ = skeletalMesh;

	OnTargetMeshChanged();
}

bool SkeletalMeshViewerPanel::HasCurrentMesh() const
{
	return targetSkeletalMesh_ && !targetSkeletalMesh_->GetSubMeshes().empty();
}

std::string SkeletalMeshViewerPanel::GetCurrentMeshPath() const
{
	return targetSkeletalMesh_ ? targetSkeletalMesh_->GetPath() : "";
}

const Box* SkeletalMeshViewerPanel::GetCurrentMeshBounds() const
{
	return targetSkeletalMesh_ ? &targetSkeletalMesh_->GetAABB() : nullptr;
}

size_t SkeletalMeshViewerPanel::GetSubMeshCount() const
{
	return targetSkeletalMesh_ ? targetSkeletalMesh_->GetSubMeshes().size() : 0;
}

std::string SkeletalMeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

size_t SkeletalMeshViewerPanel::GetSubMeshVertexCount(size_t subMeshIndex) const
{
	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetVertexCount() : 0;
}

size_t SkeletalMeshViewerPanel::GetSubMeshFaceCount(size_t subMeshIndex) const
{
	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
	return subMesh ? subMesh->GetFaceCount() : 0;
}

bool SkeletalMeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
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

MaterialInstance* SkeletalMeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
	Material* material = subMesh ? subMesh->GetMaterial() : nullptr;
	return material ? MaterialInstance::Create(material) : nullptr;
}

void SkeletalMeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
	if (!skeletalMeshComponent_ || !targetSkeletalMesh_)
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (!meshInstance || subMeshIndex >= targetSkeletalMesh_->GetSubMeshes().size())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	meshInstance->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);
}

const char* SkeletalMeshViewerPanel::GetNoMeshSelectedText() const
{
	return "No skeletal mesh selected.";
}

bool SkeletalMeshViewerPanel::HasAdditionalSidePanelContent() const
{
	return true;
}

void SkeletalMeshViewerPanel::DrawAdditionalSidePanelContent()
{
	ImGui::Text("Animations");

	if (!targetSkeletalMesh_)
	{
		ImGui::TextDisabled("No skeletal mesh selected.");
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_ ? skeletalMeshComponent_->GetMeshInstance() : nullptr;
	if (!meshInstance)
	{
		ImGui::TextDisabled("Animation preview is not ready.");
		return;
	}

	const auto& animationsMap = targetSkeletalMesh_->GetAnimationsMap();
	if (animationsMap.empty())
	{
		ImGui::TextDisabled("No animations found.");
		return;
	}

	const auto& currentAnimation = meshInstance->GetSkeletalMeshAnimation();
	for (const auto& animationPair : animationsMap)
	{
		const std::string& animationName = animationPair.first;
		const bool isSelected =
			currentAnimation.skeletalAnimation &&
			currentAnimation.skeletalAnimation->name == animationName;

		if (ImGui::Selectable(animationName.c_str(), isSelected))
		{
			meshInstance->PlayAnimation(animationName);
		}
	}
}

void SkeletalMeshViewerPanel::ClearPreviewMaterialOverrides()
{
	if (!skeletalMeshComponent_)
	{
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	SkeletalMesh* currentMesh = meshInstance ? meshInstance->GetMesh() : nullptr;
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

SkeletalMeshUnit* SkeletalMeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetSkeletalMesh_ || subMeshIndex >= targetSkeletalMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetSkeletalMesh_->GetSubMeshes()[subMeshIndex];
}
