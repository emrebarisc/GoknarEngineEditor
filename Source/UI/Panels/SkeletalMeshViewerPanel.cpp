#include "pch.h"

#include "SkeletalMeshViewerPanel.h"

#include "imgui.h"

#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Model/SkeletalMesh.h"
#include "Model/SkeletalMeshInstance.h"

constexpr unsigned int SKELETAL_MESH_VIEWER_RENDER_MASK = 0x40000000;

SkeletalMeshViewerPanel::SkeletalMeshViewerPanel(EditorHUD* hud) :
	MeshAssetViewerPanelBase(
		"Skeletal Mesh Viewer",
		hud, 
		"__Editor__SkeletalMeshViewerCamera", 
		"__Editor__SkeletalMeshViewerTarget", 
		SKELETAL_MESH_VIEWER_RENDER_MASK, 
		"SkeletalMeshViewportArea", 
		"SkeletalMeshSideArea")
{
	skeletalMeshComponent_ = viewedObject_->AddSubComponent<SkeletalMeshComponent>();
}

SkeletalMeshViewerPanel::~SkeletalMeshViewerPanel() = default;

void SkeletalMeshViewerPanel::DrawAdditionalSidePanelContent()
{
	ImGui::Text("Available Animations");
	ImGui::Separator();

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (meshInstance && meshInstance->GetMesh())
	{
		SkeletalMesh* skeletalMesh = meshInstance->GetMesh();
		const auto& animationsMap = skeletalMesh->GetAnimationsMap();

		if (animationsMap.empty())
		{
			ImGui::TextDisabled("No animations found.");
		}
		else
		{
			for (const auto& pair : animationsMap)
			{
				const std::string& animName = pair.first;

				// Check if this animation is the currently playing one
				bool isSelected = false;
				const auto& currentAnimData = meshInstance->GetSkeletalMeshAnimation();
				if (currentAnimData.skeletalAnimation && currentAnimData.skeletalAnimation->name == animName)
				{
					isSelected = true;
				}

				if (ImGui::Selectable(animName.c_str(), isSelected))
				{
					meshInstance->PlayAnimation(animName);
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No Skeletal Mesh selected.");
	}
}

void SkeletalMeshViewerPanel::SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh)
{
	skeletalMeshComponent_->SetMesh(skeletalMesh);
	skeletalMeshComponent_->SetIsActive(skeletalMesh != nullptr);

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (meshInstance)
	{
		meshInstance->SetRenderMask(SKELETAL_MESH_VIEWER_RENDER_MASK);
	}
	OnTargetMeshChanged();
}

bool SkeletalMeshViewerPanel::HasCurrentMesh() const
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	return skeletalMesh && !skeletalMesh->GetSubMeshes().empty();
}

std::string SkeletalMeshViewerPanel::GetCurrentMeshPath() const
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	return skeletalMesh ? skeletalMesh->GetPath() : "";
}

const Box* SkeletalMeshViewerPanel::GetCurrentMeshBounds() const
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	return skeletalMesh ? &skeletalMesh->GetAABB() : nullptr;
}

size_t SkeletalMeshViewerPanel::GetSubMeshCount() const
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	return skeletalMesh ? skeletalMesh->GetSubMeshes().size() : 0;
}

std::string SkeletalMeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	if (!skeletalMesh || subMeshIndex >= skeletalMesh->GetSubMeshes().size())
	{
		return "";
	}

	return skeletalMesh->GetSubMeshes()[subMeshIndex]->GetName();
}

bool SkeletalMeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	if (!skeletalMesh || subMeshIndex >= skeletalMesh->GetSubMeshes().size())
	{
		return false;
	}

	auto* subMesh = skeletalMesh->GetSubMeshes()[subMeshIndex];
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
	SkeletalMesh* skeletalMesh = GetCurrentSkeletalMesh();
	if (!skeletalMesh || subMeshIndex >= skeletalMesh->GetSubMeshes().size())
	{
		return nullptr;
	}

	Material* material = skeletalMesh->GetSubMeshes()[subMeshIndex]->GetMaterial();
	return material ? MaterialInstance::Create(material) : nullptr;
}

void SkeletalMeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
	skeletalMeshComponent_->GetMeshInstance()->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);
}

const char* SkeletalMeshViewerPanel::GetNoMeshSelectedText() const
{
	return "No skeletal mesh selected.";
}

bool SkeletalMeshViewerPanel::HasAdditionalSidePanelContent() const
{
	return true;
}

float SkeletalMeshViewerPanel::GetStackedSidePanelHeight() const
{
	return 340.f;
}

SkeletalMesh* SkeletalMeshViewerPanel::GetCurrentSkeletalMesh() const
{
	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_ ? skeletalMeshComponent_->GetMeshInstance() : nullptr;
	return meshInstance ? meshInstance->GetMesh() : nullptr;
}
