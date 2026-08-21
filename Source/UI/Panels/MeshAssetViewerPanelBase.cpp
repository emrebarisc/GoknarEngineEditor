#include "MeshAssetViewerPanelBase.h"

#include <algorithm>
#include <array>
#include <cmath>
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
#include "Goknar/Materials/MaterialSerializer.h"
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
		return { 0.62f, 0.66f, 0.70f, 1.f };
	}

	std::string GetDisplayAssetPath(const std::string& assetPath)
	{
		const std::string contentRelativePath = EditorAssetPathUtils::ToContentRelativePath(assetPath);
		return contentRelativePath.empty() ? assetPath : contentRelativePath;
	}

	std::vector<std::string> ResolveMaterialPathsForSubMeshes(std::vector<std::string> materialPaths, size_t subMeshCount)
	{
	if (materialPaths.size() == 1 && !materialPaths[0].empty() && 1 < subMeshCount)
	{
		const std::string sharedMaterialPath = materialPaths[0];
		materialPaths.assign(subMeshCount, sharedMaterialPath);
		return materialPaths;
	}

		materialPaths.resize(subMeshCount);
		return materialPaths;
	}

	Vector4 HsvToRgb(float hue, float saturation, float value)
	{
		const float scaledHue = hue * 6.f;
		const int sector = static_cast<int>(std::floor(scaledHue));
		const float localHue = scaledHue - static_cast<float>(sector);
		const float p = value * (1.f - saturation);
		const float q = value * (1.f - saturation * localHue);
		const float t = value * (1.f - saturation * (1.f - localHue));

		switch (sector % 6)
		{
		case 0: return { value, t, p, 1.f };
		case 1: return { q, value, p, 1.f };
		case 2: return { p, value, t, 1.f };
		case 3: return { p, q, value, 1.f };
		case 4: return { t, p, value, 1.f };
		default: return { value, p, q, 1.f };
		}
	}

	Vector4 GetMaterialSlotColor(size_t subMeshIndex)
	{
		static const std::array<Vector4, 12> MaterialSlotColors
		{
			Vector4(0.92f, 0.28f, 0.22f, 1.f),
			Vector4(0.20f, 0.58f, 0.96f, 1.f),
			Vector4(0.18f, 0.74f, 0.42f, 1.f),
			Vector4(0.96f, 0.70f, 0.22f, 1.f),
			Vector4(0.68f, 0.42f, 0.94f, 1.f),
			Vector4(0.94f, 0.38f, 0.68f, 1.f),
			Vector4(0.18f, 0.78f, 0.78f, 1.f),
			Vector4(0.86f, 0.50f, 0.18f, 1.f),
			Vector4(0.50f, 0.80f, 0.24f, 1.f),
			Vector4(0.36f, 0.46f, 0.92f, 1.f),
			Vector4(0.92f, 0.34f, 0.36f, 1.f),
			Vector4(0.24f, 0.70f, 0.58f, 1.f)
		};

		if (subMeshIndex < MaterialSlotColors.size())
		{
			return MaterialSlotColors[subMeshIndex];
		}

		const float hue = std::fmod(0.61803398875f * static_cast<float>(subMeshIndex + 1), 1.f);
		return HsvToRgb(hue, 0.72f, 0.94f);
	}

	ImVec4 ToImGuiColor(const Vector4& color)
	{
		return ImVec4(color.x, color.y, color.z, color.w);
	}

	Vector4 BlendColors(const Vector4& a, const Vector4& b, float alpha)
	{
		const float inverseAlpha = 1.f - alpha;
		return Vector4(
			a.x * inverseAlpha + b.x * alpha,
			a.y * inverseAlpha + b.y * alpha,
			a.z * inverseAlpha + b.z * alpha,
			a.w * inverseAlpha + b.w * alpha);
	}

	void ApplyPreviewGridShader(Material* material)
	{
		if (!material || !material->GetInitializationData())
		{
			return;
		}

		MaterialInitializationData* initData = material->GetInitializationData();
		initData->baseColor.calculation = R"(
	mat4 editorPreviewModelMatrix = finalModelMatrix;
	vec3 editorPreviewObjectScale = vec3(
		length(editorPreviewModelMatrix[0].xyz),
		length(editorPreviewModelMatrix[1].xyz),
		length(editorPreviewModelMatrix[2].xyz)
	);

	vec3 editorPreviewLocalPosition = (fragmentPositionWorldSpace * inverse(editorPreviewModelMatrix)).xyz;
	vec3 editorPreviewWorldSizedPosition = editorPreviewLocalPosition * editorPreviewObjectScale;
	vec3 editorPreviewAbsNormal = abs(normalize(vertexNormal));
	float editorPreviewMaxNormalComponent = max(editorPreviewAbsNormal.x, max(editorPreviewAbsNormal.y, editorPreviewAbsNormal.z));

	vec2 editorPreviewWorldGridUv;
	if (editorPreviewAbsNormal.x >= editorPreviewMaxNormalComponent)
	{
		editorPreviewWorldGridUv = editorPreviewWorldSizedPosition.zy;
	}
	else if (editorPreviewAbsNormal.y >= editorPreviewMaxNormalComponent)
	{
		editorPreviewWorldGridUv = editorPreviewWorldSizedPosition.xz;
	}
	else
	{
		editorPreviewWorldGridUv = editorPreviewWorldSizedPosition.xy;
	}

	vec2 editorPreviewUv = textureUV;
	vec2 editorPreviewUvDerivative = abs(dFdx(editorPreviewUv)) + abs(dFdy(editorPreviewUv));
	if (editorPreviewUvDerivative.x + editorPreviewUvDerivative.y < 0.00001f)
	{
		editorPreviewUv = editorPreviewWorldGridUv;
	}

	vec2 editorPreviewGridUv = editorPreviewUv * 7.5f;
	vec2 editorPreviewMinorFraction = fract(editorPreviewGridUv);
	vec2 editorPreviewMinorDistance = min(editorPreviewMinorFraction, vec2(1.f) - editorPreviewMinorFraction);
	vec2 editorPreviewMinorDerivative = max(fwidth(editorPreviewGridUv), vec2(0.0001f));
	vec2 editorPreviewMinorMask2 = vec2(1.f) - smoothstep(
		vec2(0.035f),
		vec2(0.035f) + editorPreviewMinorDerivative,
		editorPreviewMinorDistance
	);
	float editorPreviewMinorMask = max(editorPreviewMinorMask2.x, editorPreviewMinorMask2.y);

	vec2 editorPreviewMajorUv = editorPreviewGridUv / 5.f;
	vec2 editorPreviewMajorFraction = fract(editorPreviewMajorUv);
	vec2 editorPreviewMajorDistance = min(editorPreviewMajorFraction, vec2(1.f) - editorPreviewMajorFraction);
	vec2 editorPreviewMajorDerivative = max(fwidth(editorPreviewMajorUv), vec2(0.0001f));
	vec2 editorPreviewMajorMask2 = vec2(1.f) - smoothstep(
		vec2(0.020f),
		vec2(0.020f) + editorPreviewMajorDerivative,
		editorPreviewMajorDistance
	);
	float editorPreviewMajorMask = max(editorPreviewMajorMask2.x, editorPreviewMajorMask2.y);

	float editorPreviewChecker = mod(floor(editorPreviewGridUv.x) + floor(editorPreviewGridUv.y), 2.f);
	vec3 editorPreviewLightCell = min(baseColor.rgb * 1.16f + vec3(0.035f), vec3(1.f));
	vec3 editorPreviewDarkCell = max(baseColor.rgb * 0.68f, vec3(0.f));
	vec3 editorPreviewGridColor = mix(editorPreviewDarkCell, editorPreviewLightCell, step(0.5f, editorPreviewChecker));
	editorPreviewGridColor = mix(editorPreviewGridColor, baseColor.rgb * 0.36f, editorPreviewMinorMask * 0.72f);
	editorPreviewGridColor = mix(editorPreviewGridColor, vec3(0.045f, 0.055f, 0.070f), editorPreviewMajorMask * 0.86f);
	vec4 editorPreviewGridResult = vec4(editorPreviewGridColor, baseColor.a);
)";
		initData->baseColor.result = "editorPreviewGridResult;";
		initData->emissiveColor.result = "editorPreviewGridResult.rgb;";
	}

	Material* CreateInitializedGridMaterial(MeshUnit* subMesh, const char* materialName, const Vector4& materialColor)
	{
		if (!subMesh)
		{
			return nullptr;
		}

		Material* material = new Material();
		material->SetName(materialName ? materialName : "__Editor__MeshViewerGridPreviewMaterial");
		material->SetBaseColor(materialColor);
		material->SetEmissiveColor(Vector3(materialColor.x, materialColor.y, materialColor.z));
		material->SetAmbientOcclusion(1.f);
		material->SetMetallic(0.f);
		material->SetRoughness(0.7f);
		material->SetShadingModel(MaterialShadingModel::TwoSided);
		material->SetShadingType(MaterialShadingType::Unlit);
		ApplyPreviewGridShader(material);
		material->Build(subMesh);
		material->PreInit();
		material->Init();
		material->PostInit();

		return material;
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

	const std::string meshPath = GetCurrentMeshPath();
	selectedMaterialPaths_ = ResolveMaterialPathsForSubMeshes(
		meshPath.empty() ? std::vector<std::string>{} : AssetParser::GetMeshMaterialPaths(meshPath),
		GetSubMeshCount());

	for (size_t subMeshIndex = 0; subMeshIndex < selectedMaterialPaths_.size(); ++subMeshIndex)
	{
		if (!selectedMaterialPaths_[subMeshIndex].empty())
		{
			RebuildCurrentMaterial(subMeshIndex, selectedMaterialPaths_[subMeshIndex]);
		}
	}

	InitializeCurrentMeshMaterials();

	if (!IsCurrentMeshReadyToView())
	{
		ResetCameraToCurrentMesh();
		SetPreviewRenderActive(false);
		return;
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

bool MeshAssetViewerPanelBase::IsMaterialSlotVisualizerEnabled() const
{
	return materialSlotVisualizerEnabled_;
}

bool MeshAssetViewerPanelBase::IsMaterialSlotUnset(size_t subMeshIndex) const
{
	return !HasMaterialAssetOverride(subMeshIndex);
}

bool MeshAssetViewerPanelBase::RebuildMaterialForSubMesh(MeshUnit* subMesh, const std::string& materialPath) const
{
	if (!subMesh || !DoesMaterialAssetExist(materialPath))
	{
		return false;
	}

	Material* material = subMesh->GetMaterial();
	if (material)
	{
		material->ResetForRebuild();
	}
	else
	{
		material = new Material();
		subMesh->SetMaterial(material);
	}

	MaterialSerializer::Deserialize(materialPath, material);
	material->Build(subMesh);
	material->PreInit();
	material->Init();
	material->PostInit();
	AssetParser::RegisterMaterialTexturesToTextureAtlas(material);

	return true;
}

void MeshAssetViewerPanelBase::InitializeMaterialForSubMesh(MeshUnit* subMesh) const
{
	Material* material = subMesh ? subMesh->GetMaterial() : nullptr;
	if (!material || material->GetIsInitialized())
	{
		return;
	}

	material->Build(subMesh);
	material->PreInit();
	material->Init();
	material->PostInit();
	AssetParser::RegisterMaterialTexturesToTextureAtlas(material);
}

Vector4 MeshAssetViewerPanelBase::GetMaterialSlotVisualizerColor(size_t subMeshIndex) const
{
	return GetMaterialSlotColor(subMeshIndex);
}

Material* MeshAssetViewerPanelBase::CreateInitializedPreviewDefaultMaterial(MeshUnit* subMesh, const char* materialName) const
{
	return CreateInitializedGridMaterial(subMesh, materialName ? materialName : "__Editor__MeshViewerDefaultGridMaterial", GetPreviewDefaultMaterialColor());
}

Material* MeshAssetViewerPanelBase::CreateInitializedMaterialSlotVisualizerMaterial(MeshUnit* subMesh, const char* materialName) const
{
	return CreateInitializedGridMaterial(subMesh, materialName ? materialName : "__Editor__MeshViewerMaterialSlotVisualizer", Vector4(1.f));
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
	materialInstance->SetRoughness(0.7f);
	materialInstance->SetShadingModel(MaterialShadingModel::TwoSided);
	materialInstance->SetShadingType(MaterialShadingType::Unlit);
	return materialInstance;
}

MaterialInstance* MeshAssetViewerPanelBase::CreateMaterialSlotVisualizerMaterialInstance(Material* material, size_t subMeshIndex, bool isMaterialUnset) const
{
	if (!material)
	{
		return nullptr;
	}

	MaterialInstance* materialInstance = MaterialInstance::Create(material);
	Vector4 slotColor = GetMaterialSlotVisualizerColor(subMeshIndex);
	if (isMaterialUnset)
	{
		slotColor = BlendColors(slotColor, GetPreviewDefaultMaterialColor(), 0.34f);
	}

	materialInstance->SetBaseColor(slotColor);
	materialInstance->SetEmissiveColor(Vector3(slotColor.x, slotColor.y, slotColor.z));
	materialInstance->SetAmbientOcclusion(1.f);
	materialInstance->SetMetallic(0.f);
	materialInstance->SetRoughness(0.7f);
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

void MeshAssetViewerPanelBase::InitializeCurrentMeshMaterials()
{
}

void MeshAssetViewerPanelBase::RefreshPreviewRenderData()
{
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
	if (ImGui::Checkbox("Visualize Material Slots", &materialSlotVisualizerEnabled_))
	{
		RefreshPreviewMaterialOverrides();
	}

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
	const Vector4 slotColor = GetMaterialSlotVisualizerColor(subMeshIndex);
	if (materialSlotVisualizerEnabled_)
	{
		ImGui::ColorButton(
			"##MaterialSlotColor",
			ToImGuiColor(slotColor),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoPicker,
			ImVec2(14.f, 14.f));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Material Slot %d", static_cast<int>(subMeshIndex));
		}
		ImGui::SameLine();
	}

	if (subMeshName.empty())
	{
		ImGui::Text("Material Slot %d", static_cast<int>(subMeshIndex));
	}
	else
	{
		ImGui::TextWrapped("Material Slot %d: %s", static_cast<int>(subMeshIndex), subMeshName.c_str());
	}

	const std::string& materialPath = selectedMaterialPaths_[subMeshIndex];
	ImGui::TextWrapped("%s", materialPath.empty() ? "Grid default material (unset)" : materialPath.c_str());

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

	RefreshPreviewRenderData();
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
