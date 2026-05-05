#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "EditorPanel.h"
#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Math/GoknarMath.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"

#include "Controllers/MeshViewerCameraController.h"
#include "Objects/MeshViewerCameraObject.h"
#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorUtils.h"
#include "UI/Panels/AssetSelectorPanel.h"

class MeshAssetViewerPanelBase : public IEditorPanel
{
public:
	MeshAssetViewerPanelBase(const std::string& title,
		EditorHUD* hud,
		const std::string& cameraObjectName,
		const std::string& viewedObjectName,
		unsigned int renderMask,
		const std::string& viewportChildId,
		const std::string& sidePanelChildId) :
		IEditorPanel(title, hud),
		size_(1024, 1024),
		renderMask_(renderMask),
		viewportChildId_(viewportChildId),
		sidePanelChildId_(sidePanelChildId)
	{
		cameraObject_ = new MeshViewerCameraObject();
		cameraObject_->SetName(cameraObjectName);
		cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
		cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(renderMask_);
		cameraObject_->SetWorldPosition({ 0.f, 0.f, 0.f });

		renderTarget_ = new RenderTarget();
		renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
		renderTarget_->SetFrameSize(size_);
		renderTarget_->SetRerenderShadowMaps(false);

		viewedObject_ = new ObjectBase();
		viewedObject_->SetName(viewedObjectName);
		viewedObject_->SetWorldPosition(Vector3::ZeroVector);

		isOpen_ = false;
	}

	virtual ~MeshAssetViewerPanelBase()
	{
		delete renderTarget_;
		cameraObject_->Destroy();
		viewedObject_->Destroy();
	}

	virtual void Init() override
	{
		renderTarget_->Init();
	}

	virtual void Draw() override
	{
		if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			cameraObject_->GetController()->SetIsActive(false);
			return;
		}

		const ImVec2 contentAvailable = ImGui::GetContentRegionAvail();
		const float sidePanelWidth = 320.f;
		const float sidePanelHeight = GetStackedSidePanelHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const bool drawPanelsSideBySide = contentAvailable.x > sidePanelWidth;
		const float viewportWidth = drawPanelsSideBySide ? contentAvailable.x - sidePanelWidth - spacing : contentAvailable.x;
		const float viewportHeight = drawPanelsSideBySide ? contentAvailable.y : std::max(100.f, contentAvailable.y - sidePanelHeight - spacing);

		ImGui::BeginChild(viewportChildId_.c_str(), ImVec2(viewportWidth, viewportHeight), true);
		ImVec2 newViewportSize = ImGui::GetContentRegionAvail();

		if (newViewportSize.x <= 0.0f || newViewportSize.y <= 0.0f)
		{
			ImGui::EndChild();
			ImGui::End();
			return;
		}

		if (size_.x != newViewportSize.x || size_.y != newViewportSize.y)
		{
			size_ = EditorUtils::ToVector2(newViewportSize);
			renderTarget_->SetFrameSize({ newViewportSize.x, newViewportSize.y });
		}

		Texture* renderTargetTexture = renderTarget_->GetTexture();
		ImGui::Image(
			(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
			EditorUtils::ToImVec2(size_),
			ImVec2{ 0.f, 1.f },
			ImVec2{ 1.f, 0.f }
		);

		cameraObject_->GetController()->SetIsActive(ImGui::IsItemHovered());
		EditorUtils::DrawWorldAxis(cameraObject_->GetCameraComponent()->GetCamera());
		ImGui::EndChild();

		if (drawPanelsSideBySide)
		{
			ImGui::SameLine();
		}

		const ImVec2 sidePanelSize = drawPanelsSideBySide ? ImVec2(sidePanelWidth, contentAvailable.y) : ImVec2(contentAvailable.x, sidePanelHeight);
		ImGui::BeginChild(sidePanelChildId_.c_str(), sidePanelSize, true);
		DrawMeshProperties();

		if (HasAdditionalSidePanelContent())
		{
			ImGui::Spacing();
			DrawAdditionalSidePanelContent();
		}

		ImGui::EndChild();
		ImGui::End();
	}

protected:
	void OnTargetMeshChanged()
	{
		pendingMaterialSelectionSubMeshIndex_ = -1;

		const std::string meshPath = GetCurrentMeshPath();
		selectedMaterialPaths_ = meshPath.empty() ? std::vector<std::string>{} : AssetParser::GetMeshMaterialPaths(meshPath);
		selectedMaterialPaths_.resize(GetSubMeshCount());

		for (size_t subMeshIndex = 0; subMeshIndex < selectedMaterialPaths_.size(); ++subMeshIndex)
		{
			if (!selectedMaterialPaths_[subMeshIndex].empty())
			{
				RebuildCurrentMaterial(subMeshIndex, selectedMaterialPaths_[subMeshIndex]);
			}
		}

		ApplyPreviewMaterialOverrides();

		if (const Box* meshBounds = GetCurrentMeshBounds())
		{
			cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, *meshBounds);
		}
	}

	ObjectBase* viewedObject_{ nullptr };

private:
	void DrawMeshProperties()
	{
		ImGui::Text("Mesh Properties");
		ImGui::Separator();

		const std::string meshPath = GetCurrentMeshPath();
		if (!HasCurrentMesh() || meshPath.empty())
		{
			ImGui::TextDisabled("%s", GetNoMeshSelectedText());
			return;
		}

		ImGui::Text("Mesh:");
		ImGui::TextWrapped("%s", EditorAssetPathUtils::ToContentRelativePath(meshPath).c_str());

		ImGui::Spacing();
		ImGui::Text("Default Materials:");

		const size_t subMeshCount = GetSubMeshCount();
		selectedMaterialPaths_.resize(subMeshCount);

		for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
		{
			ImGui::PushID(static_cast<int>(subMeshIndex));

			const std::string subMeshName = GetSubMeshName(subMeshIndex);
			if (subMeshName.empty())
			{
				ImGui::Text("Sub Mesh %d", static_cast<int>(subMeshIndex));
			}
			else
			{
				ImGui::Text("Sub Mesh %d: %s", static_cast<int>(subMeshIndex), subMeshName.c_str());
			}

			ImGui::TextWrapped("%s", selectedMaterialPaths_[subMeshIndex].empty() ? "None" : selectedMaterialPaths_[subMeshIndex].c_str());

			if (ImGui::Button("Select Asset"))
			{
				pendingMaterialSelectionSubMeshIndex_ = static_cast<int>(subMeshIndex);
				EditorContext::Get()->assetSelectorFilter = EditorAssetType::Material;
				AssetSelectorPanel::OnAssetSelected =
					Delegate<void(const std::string&)>::Create<MeshAssetViewerPanelBase, &MeshAssetViewerPanelBase::OnMaterialSelected>(this);

				hud_->ShowPanel<AssetSelectorPanel>();
			}

			ImGui::PopID();

			if (subMeshIndex + 1 < subMeshCount)
			{
				ImGui::Separator();
			}
		}
	}

	void OnMaterialSelected(const std::string& path)
	{
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;

		const int selectedSubMeshIndex = pendingMaterialSelectionSubMeshIndex_;
		pendingMaterialSelectionSubMeshIndex_ = -1;

		const std::string meshPath = GetCurrentMeshPath();
		if (!HasCurrentMesh() || meshPath.empty())
		{
			return;
		}

		if (selectedSubMeshIndex < 0 || selectedSubMeshIndex >= static_cast<int>(GetSubMeshCount()))
		{
			return;
		}

		selectedMaterialPaths_.resize(GetSubMeshCount());
		selectedMaterialPaths_[selectedSubMeshIndex] = EditorAssetPathUtils::ToContentRelativePath(path);
		if (selectedMaterialPaths_[selectedSubMeshIndex].empty())
		{
			return;
		}

		if (!RebuildCurrentMaterial(static_cast<size_t>(selectedSubMeshIndex), selectedMaterialPaths_[selectedSubMeshIndex]))
		{
			return;
		}

		AssetParser::SetMeshMaterialPaths(meshPath, selectedMaterialPaths_);
		ApplyPreviewMaterialOverrides();
	}

	void ApplyPreviewMaterialOverrides()
	{
		const size_t subMeshCount = GetSubMeshCount();
		for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
		{
			MaterialInstance* previewMaterialInstance = CreatePreviewMaterialInstance(subMeshIndex);
			if (previewMaterialInstance)
			{
				previewMaterialInstance->SetEmisiveColor({ 1.f });
			}

			SetPreviewMaterial(subMeshIndex, previewMaterialInstance);
		}
	}

	virtual bool HasCurrentMesh() const = 0;
	virtual std::string GetCurrentMeshPath() const = 0;
	virtual const Box* GetCurrentMeshBounds() const = 0;
	virtual size_t GetSubMeshCount() const = 0;
	virtual std::string GetSubMeshName(size_t subMeshIndex) const = 0;
	virtual bool RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath) = 0;
	virtual MaterialInstance* CreatePreviewMaterialInstance(size_t subMeshIndex) const = 0;
	virtual void SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance) = 0;
	virtual const char* GetNoMeshSelectedText() const = 0;
	virtual bool HasAdditionalSidePanelContent() const
	{
		return false;
	}

	virtual void DrawAdditionalSidePanelContent()
	{
	}

	virtual float GetStackedSidePanelHeight() const
	{
		return 240.f;
	}

	RenderTarget* renderTarget_{ nullptr };
	MeshViewerCameraObject* cameraObject_{ nullptr };

	Vector2 size_;
	std::vector<std::string> selectedMaterialPaths_{};
	int pendingMaterialSelectionSubMeshIndex_{ -1 };
	unsigned int renderMask_{ 0 };
	std::string viewportChildId_{};
	std::string sidePanelChildId_{};
};
