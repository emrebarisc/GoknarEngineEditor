#include "SkeletalMeshViewerPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Model/MeshContainer.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/SkeletalMeshInstance.h"
#include "Goknar/Renderer/Renderer.h"

namespace
{
	constexpr unsigned int SkeletalMeshViewerRenderMask = 0x40000000;

	bool HasSkeletalMeshSelection(const SkeletalMesh* skeletalMesh)
	{
		return skeletalMesh && !skeletalMesh->GetSubMeshes().empty();
	}

	bool IsSkeletalMeshReadyForPreview(const SkeletalMesh* skeletalMesh)
	{
		if (!HasSkeletalMeshSelection(skeletalMesh))
		{
			return false;
		}

		for (const SkeletalMeshUnit* subMesh : skeletalMesh->GetSubMeshes())
		{
			if (!subMesh || subMesh->GetVertexCount() == 0 || subMesh->GetFaceCount() == 0)
			{
				return false;
			}
		}

		return true;
	}
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
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
}

void SkeletalMeshViewerPanel::SetTargetSkeletalMesh(SkeletalMeshContainer* skeletalMeshContainer)
{
	ClearPreviewMaterialOverrides();
	ClearPreviewDefaultMaterial();
	ClearMaterialSlotVisualizerMaterial();
	targetSkeletalMeshContainer_ = nullptr;
	targetSkeletalMesh_ = nullptr;

	if (!skeletalMeshContainer)
	{
		skeletalMeshComponent_->SetIsActive(false);
		OnTargetMeshChanged();
		return;
	}

	targetSkeletalMeshContainer_ = skeletalMeshContainer;
	targetSkeletalMesh_ = skeletalMeshContainer->GetLOD(0);

	if (IsSkeletalMeshReadyForPreview(targetSkeletalMesh_))
	{
		skeletalMeshComponent_->SetMesh(skeletalMeshContainer);
		skeletalMeshComponent_->SetIsActive(true);
		skeletalMeshComponent_->GetMeshInstance()->SetRenderMask(GetRenderMask());
		skeletalMeshComponent_->GetMeshInstance()->SetIsCastingShadow(false);
		skeletalMeshComponent_->GetMeshInstance()->SetForcedLODIndex(-1);
	}
	else
	{
		skeletalMeshComponent_->SetIsActive(false);
	}

	OnTargetMeshChanged();
}

bool SkeletalMeshViewerPanel::HasCurrentMesh() const
{
	return HasSkeletalMeshSelection(targetSkeletalMesh_);
}

bool SkeletalMeshViewerPanel::IsCurrentMeshReadyToView() const
{
	return IsSkeletalMeshReadyForPreview(targetSkeletalMesh_);
}

std::string SkeletalMeshViewerPanel::GetCurrentMeshPath() const
{
	return targetSkeletalMeshContainer_ ? targetSkeletalMeshContainer_->GetPath() : "";
}

const Box* SkeletalMeshViewerPanel::GetCurrentMeshBounds() const
{
	return targetSkeletalMesh_ ? &targetSkeletalMesh_->GetAABB() : nullptr;
}

const Box* SkeletalMeshViewerPanel::GetCurrentMeshCoverageBounds() const
{
	return targetSkeletalMeshContainer_ ? &targetSkeletalMeshContainer_->GetAABB() : nullptr;
}

const Matrix* SkeletalMeshViewerPanel::GetCurrentMeshWorldTransformationMatrix() const
{
	return skeletalMeshComponent_ ? &skeletalMeshComponent_->GetComponentToWorldTransformationMatrix() : nullptr;
}

size_t SkeletalMeshViewerPanel::GetLODCount() const
{
	return targetSkeletalMeshContainer_ ? targetSkeletalMeshContainer_->GetLODCount() : 0;
}

size_t SkeletalMeshViewerPanel::GetLODIndexForFrameCoverage(float frameCoverage) const
{
	return targetSkeletalMeshContainer_ ? targetSkeletalMeshContainer_->GetLODIndex(frameCoverage) : 0;
}

float SkeletalMeshViewerPanel::GetLODFrameCoverage(size_t LODIndex) const
{
	return targetSkeletalMeshContainer_ ? targetSkeletalMeshContainer_->GetLODFrameCoverage((int)LODIndex) : 0.f;
}

void SkeletalMeshViewerPanel::SetLODFrameCoverage(size_t LODIndex, float frameCoverage)
{
	if (targetSkeletalMeshContainer_)
	{
		targetSkeletalMeshContainer_->SetLODFrameCoverage((int)LODIndex, frameCoverage);
	}
}

bool SkeletalMeshViewerPanel::SetCurrentLODIndex(size_t LODIndex)
{
	if (!targetSkeletalMeshContainer_)
	{
		targetSkeletalMesh_ = nullptr;
		return false;
	}

	ClearPreviewMaterialOverrides();
	targetSkeletalMesh_ = targetSkeletalMeshContainer_->GetLOD((int)LODIndex);

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_ ? skeletalMeshComponent_->GetMeshInstance() : nullptr;
	if (meshInstance)
	{
		meshInstance->SetForcedLODIndex(IsLODSelectionAutomatic() ? -1 : static_cast<int>(LODIndex));
	}

	return targetSkeletalMesh_ != nullptr;
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

size_t SkeletalMeshViewerPanel::GetLODSubMeshCount(size_t LODIndex) const
{
	if (!targetSkeletalMeshContainer_ || GetLODCount() <= LODIndex)
	{
		return 0;
	}

	SkeletalMesh* LODMesh = targetSkeletalMeshContainer_->GetLOD((int)LODIndex);
	return LODMesh ? LODMesh->GetSubMeshes().size() : 0;
}

std::string SkeletalMeshViewerPanel::GetLODSubMeshName(size_t LODIndex, size_t subMeshIndex) const
{
	SkeletalMeshUnit* subMesh = GetLODSubMesh(LODIndex, subMeshIndex);
	return subMesh ? subMesh->GetName() : "";
}

bool SkeletalMeshViewerPanel::RebuildMaterial(size_t LODIndex, size_t subMeshIndex, const std::string& materialPath)
{
	SkeletalMeshUnit* subMesh = GetLODSubMesh(LODIndex, subMeshIndex);
	if (!targetSkeletalMeshContainer_ || !subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	if (LODIndex == GetSelectedLODIndex())
	{
		RestorePreviewSourceMaterials();
	}

	const bool rebuilt = RebuildMaterialForSubMesh(subMesh, materialPath);
	if (rebuilt && LODIndex == GetSelectedLODIndex() && subMeshIndex < previewSourceMaterials_.size())
	{
		previewSourceMaterials_[subMeshIndex] = subMesh->GetMaterial();
	}

	return rebuilt;
}

bool SkeletalMeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	return RebuildMaterial(GetSelectedLODIndex(), subMeshIndex, materialPath);
}

MaterialInstance* SkeletalMeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
	if (!IsCurrentMeshReadyToView())
	{
		return nullptr;
	}

	SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
	Material* material = GetPreviewSourceMaterial(subMeshIndex);
	if (!material && subMesh)
	{
		material = subMesh->GetMaterial();
	}

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

	if (material)
	{
		return MaterialInstance::Create(material);
	}

	return CreatePreviewDefaultMaterialInstance(GetPreviewDefaultMaterial(subMesh));
}

void SkeletalMeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
	if (!skeletalMeshComponent_ || !targetSkeletalMesh_ || !IsCurrentMeshReadyToView())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (!meshInstance ||
		subMeshIndex >= targetSkeletalMesh_->GetSubMeshes().size() ||
		subMeshIndex >= meshInstance->GetMaterials().size())
	{
		if (materialInstance)
		{
			materialInstance->Destroy();
		}
		return;
	}

	CapturePreviewSourceMaterialsIfNeeded();

	meshInstance->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);

	if (SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex))
	{
		Material* parentMaterial = materialInstance ? materialInstance->GetParentMaterial() : nullptr;
		subMesh->SetMaterial(parentMaterial ? parentMaterial : GetPreviewSourceMaterial(subMeshIndex));
	}
}

void SkeletalMeshViewerPanel::RefreshPreviewRenderData()
{
	if (!skeletalMeshComponent_ || !targetSkeletalMesh_ || !IsCurrentMeshReadyToView())
	{
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (!meshInstance || !meshInstance->GetMesh())
	{
		return;
	}

	engine->GetRenderer()->RemoveSkeletalMeshInstance(meshInstance);
	engine->GetRenderer()->AddSkeletalMeshInstance(meshInstance);
}

const char* SkeletalMeshViewerPanel::GetNoMeshSelectedText() const
{
	return "No skeletal mesh selected.";
}

const char* SkeletalMeshViewerPanel::GetMeshNotReadyText() const
{
	return "Skeletal mesh is not ready to view. It has not been sent to the GPU yet.";
}

void SkeletalMeshViewerPanel::InitializeCurrentMeshMaterials()
{
	if (!targetSkeletalMesh_)
	{
		return;
	}

	for (SkeletalMeshUnit* subMesh : targetSkeletalMesh_->GetSubMeshes())
	{
		InitializeMaterialForSubMesh(subMesh);
	}
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

	if (!IsCurrentMeshReadyToView())
	{
		ImGui::TextDisabled("%s", GetMeshNotReadyText());
		return;
	}

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_ ? skeletalMeshComponent_->GetMeshInstance() : nullptr;
	if (!meshInstance)
	{
		ImGui::TextDisabled("Animation preview is not ready.");
		return;
	}

	SkeletalMesh* animationMesh = targetSkeletalMeshContainer_ ? targetSkeletalMeshContainer_->GetLOD(0) : targetSkeletalMesh_;
	if (!animationMesh)
	{
		ImGui::TextDisabled("No animations found.");
		return;
	}

	const auto& animationsMap = animationMesh->GetAnimationsMap();
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
	SkeletalMeshContainer* currentMeshContainer = meshInstance ? meshInstance->GetMesh() : nullptr;
	SkeletalMesh* currentMesh = currentMeshContainer ? currentMeshContainer->GetLOD(0) : nullptr;
	if (!meshInstance || !currentMesh)
	{
		return;
	}

	for (size_t subMeshIndex = 0; subMeshIndex < meshInstance->GetMaterials().size(); ++subMeshIndex)
	{
		meshInstance->SetMaterial(static_cast<int>(subMeshIndex), nullptr);
	}

	RestorePreviewSourceMaterials();
	previewSourceMaterials_.clear();
}

void SkeletalMeshViewerPanel::ClearPreviewDefaultMaterial()
{
	DestroyPreviewDefaultMaterial(previewDefaultMaterial_);
}

void SkeletalMeshViewerPanel::ClearMaterialSlotVisualizerMaterial()
{
	DestroyPreviewDefaultMaterial(materialSlotVisualizerMaterial_);
}

void SkeletalMeshViewerPanel::CapturePreviewSourceMaterialsIfNeeded()
{
	if (previewMaterialsApplied_ || !targetSkeletalMesh_)
	{
		return;
	}

	const size_t subMeshCount = targetSkeletalMesh_->GetSubMeshes().size();
	previewSourceMaterials_.resize(subMeshCount, nullptr);
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex);
		previewSourceMaterials_[subMeshIndex] = subMesh ? subMesh->GetMaterial() : nullptr;
	}

	previewMaterialsApplied_ = true;
}

void SkeletalMeshViewerPanel::RestorePreviewSourceMaterials()
{
	if (!previewMaterialsApplied_ || !targetSkeletalMesh_)
	{
		return;
	}

	const size_t subMeshCount = targetSkeletalMesh_->GetSubMeshes().size();
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount && subMeshIndex < previewSourceMaterials_.size(); ++subMeshIndex)
	{
		if (SkeletalMeshUnit* subMesh = GetSubMesh(subMeshIndex))
		{
			subMesh->SetMaterial(previewSourceMaterials_[subMeshIndex]);
		}
	}

	previewMaterialsApplied_ = false;
}

SkeletalMeshUnit* SkeletalMeshViewerPanel::GetLODSubMesh(size_t LODIndex, size_t subMeshIndex) const
{
	if (!targetSkeletalMeshContainer_ || GetLODCount() <= LODIndex)
	{
		return nullptr;
	}

	SkeletalMesh* LODMesh = targetSkeletalMeshContainer_->GetLOD((int)LODIndex);
	if (!LODMesh || subMeshIndex >= LODMesh->GetSubMeshes().size())
	{
		return nullptr;
	}

	return LODMesh->GetSubMeshes()[subMeshIndex];
}

SkeletalMeshUnit* SkeletalMeshViewerPanel::GetSubMesh(size_t subMeshIndex) const
{
	if (!targetSkeletalMesh_ || subMeshIndex >= targetSkeletalMesh_->GetSubMeshes().size())
	{
		return nullptr;
	}

	return targetSkeletalMesh_->GetSubMeshes()[subMeshIndex];
}

Material* SkeletalMeshViewerPanel::GetPreviewSourceMaterial(size_t subMeshIndex) const
{
	if (subMeshIndex < previewSourceMaterials_.size())
	{
		return previewSourceMaterials_[subMeshIndex];
	}

	return nullptr;
}

Material* SkeletalMeshViewerPanel::GetPreviewDefaultMaterial(SkeletalMeshUnit* subMesh) const
{
	if (!previewDefaultMaterial_)
	{
		previewDefaultMaterial_ = CreateInitializedPreviewDefaultMaterial(subMesh, "__Editor__SkeletalMeshViewerDefaultMaterial");
	}

	return previewDefaultMaterial_;
}

Material* SkeletalMeshViewerPanel::GetMaterialSlotVisualizerMaterial(SkeletalMeshUnit* subMesh) const
{
	if (!materialSlotVisualizerMaterial_)
	{
		materialSlotVisualizerMaterial_ = CreateInitializedMaterialSlotVisualizerMaterial(subMesh, "__Editor__SkeletalMeshViewerMaterialSlotVisualizer");
	}

	return materialSlotVisualizerMaterial_;
}
