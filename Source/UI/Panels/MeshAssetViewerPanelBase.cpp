#include "MeshAssetViewerPanelBase.h"

#include <algorithm>
#include <filesystem>

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Engine.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Model/MeshUnit.h"
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

namespace
{
	constexpr float SidePanelWidth = 320.f;
	constexpr float MinimumViewportSize = 96.f;
	constexpr ImGuiTableFlags ViewerTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;

	Vector4 GetPreviewDefaultMaterialColor()
	{
		return { 0.78f, 0.84f, 0.92f, 1.f };
	}

	std::string GetDisplayAssetPath(const std::string& assetPath)
	{
		const std::string contentRelativePath = EditorAssetPathUtils::ToContentRelativePath(assetPath);
		return contentRelativePath.empty() ? assetPath : contentRelativePath;
	}
}

MeshAssetViewerPanelBase::MeshAssetViewerPanelBase(
	const std::string& title,
	EditorHUD* hud,
	const std::string& cameraObjectName,
	const std::string& viewedObjectName,
	unsigned int renderMask,
	const std::string& viewportChildId,
	const std::string& sidePanelChildId) :
	IEditorPanel(title, hud),
	renderMask_(renderMask),
	viewportChildId_(viewportChildId),
	sidePanelChildId_(sidePanelChildId)
{
	cameraObject_ = new MeshViewerCameraObject();
	cameraObject_->SetName(cameraObjectName);
	cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
	cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(renderMask_);
	cameraObject_->SetWorldPosition({ 0.f, 0.f, 90.f });
	cameraObject_->GetController()->SetIsActive(false);

	renderTarget_ = new RenderTarget();
	renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
	renderTarget_->SetRerenderShadowMaps(false);
	renderTarget_->SetIsActive(false);

	viewedObject_ = new ObjectBase();
	viewedObject_->SetName(viewedObjectName);
	viewedObject_->SetWorldPosition({ 0.f, 0.f, 100.f });

	isOpen_ = false;
}

MeshAssetViewerPanelBase::~MeshAssetViewerPanelBase()
{
	SetPreviewRenderActive(false);

	delete renderTarget_;
	renderTarget_ = nullptr;

	if (cameraObject_)
	{
		cameraObject_->Destroy();
		cameraObject_ = nullptr;
	}

	if (viewedObject_)
	{
		viewedObject_->Destroy();
		viewedObject_ = nullptr;
	}
}

void MeshAssetViewerPanelBase::Init()
{
	renderTarget_->Init();
	renderTarget_->SetFrameSize(viewportSize_);
	ResetCameraToCurrentMesh();
}

void MeshAssetViewerPanelBase::SetIsOpen(bool isOpen)
{
	IEditorPanel::SetIsOpen(isOpen);

	if (!isOpen_)
	{
		SetPreviewRenderActive(false);
	}
	else
	{
		SetPreviewRenderActive(CanRenderCurrentMesh());
	}
}

void MeshAssetViewerPanelBase::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1000.f, 640.f), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		SetPreviewRenderActive(false);
		return;
	}

	SetPreviewRenderActive(CanRenderCurrentMesh());

	if (ImGui::BeginTable("MeshAssetViewerLayout", 2, ViewerTableFlags))
	{
		ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed, SidePanelWidth);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		DrawViewport();

		ImGui::TableSetColumnIndex(1);
		DrawSidePanel();

		ImGui::EndTable();
	}

	ImGui::End();

	if (!isOpen_)
	{
		SetPreviewRenderActive(false);
	}
}

void MeshAssetViewerPanelBase::OnTargetMeshChanged()
{
	pendingMaterialSelectionSubMeshIndex_ = -1;
	selectedMaterialPaths_.clear();

	if (!HasCurrentMesh())
	{
		SetPreviewRenderActive(false);
		return;
	}

	if (!IsCurrentMeshReadyToView())
	{
		ResetCameraToCurrentMesh();
		SetPreviewRenderActive(false);
		return;
	}

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

	RefreshPreviewMaterialOverrides();
	ResetCameraToCurrentMesh();
	SetPreviewRenderActive(isOpen_);
}

bool MeshAssetViewerPanelBase::DoesMaterialAssetExist(const std::string& materialPath) const
{
	const std::string relativeMaterialPath = ContentPathUtils::ToContentRelativePath(materialPath);
	return !relativeMaterialPath.empty() &&
		std::filesystem::exists(ContentPathUtils::ToAbsoluteContentPath(relativeMaterialPath));
}

bool MeshAssetViewerPanelBase::HasMaterialAssetOverride(size_t subMeshIndex) const
{
	return subMeshIndex < selectedMaterialPaths_.size() && !selectedMaterialPaths_[subMeshIndex].empty();
}

Material* MeshAssetViewerPanelBase::CreateInitializedPreviewDefaultMaterial(MeshUnit* subMesh, const char* materialName) const
{
	if (!subMesh)
	{
		return nullptr;
	}

	Material* material = new Material();
	const Vector4 previewColor = GetPreviewDefaultMaterialColor();
	material->SetName(materialName ? materialName : "__Editor__MeshViewerDefaultPreviewMaterial");
	material->SetBaseColor(previewColor);
	material->SetEmissiveColor(Vector3(previewColor.x, previewColor.y, previewColor.z));
	material->SetAmbientOcclusion(1.f);
	material->SetMetallic(0.f);
	material->SetRoughness(0.65f);
	material->SetShadingModel(MaterialShadingModel::TwoSided);
	material->SetShadingType(MaterialShadingType::Unlit);
	material->Build(subMesh);
	material->PreInit();
	material->Init();
	material->PostInit();

	return material;
}

MaterialInstance* MeshAssetViewerPanelBase::CreatePreviewDefaultMaterialInstance(Material* material) const
{
	if (!material)
	{
		return nullptr;
	}

	MaterialInstance* materialInstance = MaterialInstance::Create(material);
	const Vector4 previewColor = GetPreviewDefaultMaterialColor();
	materialInstance->SetBaseColor(previewColor);
	materialInstance->SetEmissiveColor(Vector3(previewColor.x, previewColor.y, previewColor.z));
	materialInstance->SetAmbientOcclusion(1.f);
	materialInstance->SetMetallic(0.f);
	materialInstance->SetRoughness(0.65f);
	materialInstance->SetShadingModel(MaterialShadingModel::TwoSided);
	materialInstance->SetShadingType(MaterialShadingType::Unlit);
	return materialInstance;
}

void MeshAssetViewerPanelBase::DestroyPreviewDefaultMaterial(Material*& material) const
{
	if (!material)
	{
		return;
	}

	engine->GetResourceManager()->RemoveMaterial(material);
	material = nullptr;
}

bool MeshAssetViewerPanelBase::HasAdditionalSidePanelContent() const
{
	return false;
}

const char* MeshAssetViewerPanelBase::GetMeshNotReadyText() const
{
	return "Mesh is not ready to view.";
}

void MeshAssetViewerPanelBase::DrawAdditionalSidePanelContent()
{
}

void MeshAssetViewerPanelBase::DrawViewport()
{
	ImGui::BeginChild(viewportChildId_.c_str(), ImVec2(0.f, 0.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (availableSize.x < MinimumViewportSize || availableSize.y < MinimumViewportSize)
	{
		SetPreviewRenderActive(false);
		ImGui::EndChild();
		return;
	}

	const ImVec2 cursorScreenPosition = ImGui::GetCursorScreenPos();
	position_ = Vector2i(static_cast<int>(cursorScreenPosition.x), static_cast<int>(cursorScreenPosition.y));
	size_ = Vector2i(static_cast<int>(availableSize.x), static_cast<int>(availableSize.y));

	if (!HasCurrentMesh())
	{
		DrawEmptyViewportMessage(GetNoMeshSelectedText());
		ImGui::EndChild();
		return;
	}

	if (!IsCurrentMeshReadyToView())
	{
		DrawEmptyViewportMessage(GetMeshNotReadyText());
		ImGui::EndChild();
		return;
	}

	if (viewportSize_.x != availableSize.x || viewportSize_.y != availableSize.y)
	{
		viewportSize_ = EditorUtils::ToVector2(availableSize);
		renderTarget_->SetFrameSize(viewportSize_);
	}

	Texture* renderTargetTexture = renderTarget_->GetTexture();
	if (!renderTargetTexture)
	{
		DrawEmptyViewportMessage("Preview is initializing.");
		ImGui::EndChild();
		return;
	}

	ImGui::Image(
		(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
		availableSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f });

	const bool viewportHovered = ImGui::IsItemHovered();
	cameraObject_->GetController()->SetIsActive(viewportHovered);
	EditorUtils::DrawWorldAxis(cameraObject_->GetCameraComponent()->GetCamera());

	ImGui::EndChild();
}

void MeshAssetViewerPanelBase::DrawSidePanel()
{
	ImGui::BeginChild(sidePanelChildId_.c_str(), ImVec2(0.f, 0.f), false);

	DrawMeshProperties();

	if (HasAdditionalSidePanelContent())
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		DrawAdditionalSidePanelContent();
	}

	ImGui::EndChild();
}

void MeshAssetViewerPanelBase::DrawMeshProperties()
{
	ImGui::Text("Mesh");
	ImGui::Separator();

	if (!HasCurrentMesh())
	{
		ImGui::TextDisabled("%s", GetNoMeshSelectedText());
		return;
	}

	const std::string meshPath = GetCurrentMeshPath();
	ImGui::TextWrapped("%s", GetDisplayAssetPath(meshPath).c_str());

	if (!IsCurrentMeshReadyToView())
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", GetMeshNotReadyText());
		return;
	}

	const size_t subMeshCount = GetSubMeshCount();
	ImGui::Spacing();
	ImGui::Text("Sub Meshes: %d", static_cast<int>(subMeshCount));

	size_t vertexCount = 0;
	size_t faceCount = 0;
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		vertexCount += GetSubMeshVertexCount(subMeshIndex);
		faceCount += GetSubMeshFaceCount(subMeshIndex);
	}
	ImGui::Text("Vertices: %d", static_cast<int>(vertexCount));
	ImGui::Text("Faces: %d", static_cast<int>(faceCount));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Text("Materials");

	selectedMaterialPaths_.resize(subMeshCount);
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		DrawMaterialSelector(subMeshIndex);
	}
}

void MeshAssetViewerPanelBase::DrawMaterialSelector(size_t subMeshIndex)
{
	ImGui::PushID(static_cast<int>(subMeshIndex));
	ImGui::Spacing();

	const std::string subMeshName = GetSubMeshName(subMeshIndex);
	if (subMeshName.empty())
	{
		ImGui::Text("Sub Mesh %d", static_cast<int>(subMeshIndex));
	}
	else
	{
		ImGui::TextWrapped("Sub Mesh %d: %s", static_cast<int>(subMeshIndex), subMeshName.c_str());
	}

	const std::string& materialPath = selectedMaterialPaths_[subMeshIndex];
	ImGui::TextWrapped("%s", materialPath.empty() ? "Default material" : materialPath.c_str());

	if (ImGui::Button("Select Asset"))
	{
		pendingMaterialSelectionSubMeshIndex_ = static_cast<int>(subMeshIndex);
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::Material;
		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<MeshAssetViewerPanelBase, &MeshAssetViewerPanelBase::OnMaterialSelected>(this);
		hud_->ShowPanel<AssetSelectorPanel>();
	}

	ImGui::PopID();
}

void MeshAssetViewerPanelBase::DrawEmptyViewportMessage(const char* message)
{
	SetPreviewRenderActive(false);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const ImVec2 availableSize = ImGui::GetContentRegionAvail();
	const ImVec2 max(min.x + availableSize.x, min.y + availableSize.y);

	drawList->AddRectFilled(min, max, IM_COL32(12, 12, 14, 255));

	const char* text = message ? message : "";
	const ImVec2 textSize = ImGui::CalcTextSize(text);
	const ImVec2 textPosition(
		min.x + std::max(0.f, (availableSize.x - textSize.x) * 0.5f),
		min.y + std::max(0.f, (availableSize.y - textSize.y) * 0.5f));
	drawList->AddText(textPosition, IM_COL32(180, 180, 180, 255), text);

	ImGui::Dummy(availableSize);
	cameraObject_->GetController()->SetIsActive(false);
}

void MeshAssetViewerPanelBase::ResetCameraToCurrentMesh()
{
	if (const Box* meshBounds = GetCurrentMeshBounds())
	{
		cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, *meshBounds);
	}
	else
	{
		cameraObject_->GetController()->ResetView();
	}
}

void MeshAssetViewerPanelBase::RefreshPreviewMaterialOverrides()
{
	if (!CanRenderCurrentMesh())
	{
		return;
	}

	const size_t subMeshCount = GetSubMeshCount();
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		SetPreviewMaterial(subMeshIndex, CreatePreviewMaterialInstance(subMeshIndex));
	}
}

void MeshAssetViewerPanelBase::SetPreviewRenderActive(bool active)
{
	const bool shouldRender = active && CanRenderCurrentMesh();

	if (renderTarget_)
	{
		renderTarget_->SetIsActive(shouldRender);
	}

	if (!shouldRender && cameraObject_)
	{
		cameraObject_->GetController()->SetIsActive(false);
	}
}

void MeshAssetViewerPanelBase::OnMaterialSelected(const std::string& path)
{
	EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;

	const int selectedSubMeshIndex = pendingMaterialSelectionSubMeshIndex_;
	pendingMaterialSelectionSubMeshIndex_ = -1;

	if (!CanRenderCurrentMesh() ||
		selectedSubMeshIndex < 0 ||
		selectedSubMeshIndex >= static_cast<int>(GetSubMeshCount()))
	{
		return;
	}

	const std::string relativeMaterialPath = EditorAssetPathUtils::ToContentRelativePath(path);
	if (relativeMaterialPath.empty() || !DoesMaterialAssetExist(relativeMaterialPath))
	{
		return;
	}

	if (!RebuildCurrentMaterial(static_cast<size_t>(selectedSubMeshIndex), relativeMaterialPath))
	{
		return;
	}

	selectedMaterialPaths_.resize(GetSubMeshCount());
	selectedMaterialPaths_[selectedSubMeshIndex] = relativeMaterialPath;

	const std::string meshPath = GetCurrentMeshPath();
	if (!meshPath.empty())
	{
		AssetParser::SetMeshMaterialPaths(meshPath, selectedMaterialPaths_);
	}

	RefreshPreviewMaterialOverrides();
}

bool MeshAssetViewerPanelBase::CanRenderCurrentMesh() const
{
	return HasCurrentMesh() && IsCurrentMeshReadyToView();
}
