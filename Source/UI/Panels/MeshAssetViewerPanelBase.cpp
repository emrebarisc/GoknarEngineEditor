#include "MeshAssetViewerPanelBase.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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

	float GetFrameCoveragePercent(float frameCoverage)
	{
		if (frameCoverage >= 1.f)
		{
			return 100.f;
		}

		return std::max(0.f, std::min(100.f, frameCoverage * 100.f));
	}

	std::string FormatFrameCoveragePercent(float frameCoverage)
	{
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "%.2f%%", GetFrameCoveragePercent(frameCoverage));
		return buffer;
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
	pendingMaterialSelectionLODIndex_ = -1;
	pendingMaterialSelectionSubMeshIndex_ = -1;
	selectedMaterialPaths_.clear();
	selectedLODIndex_ = 0;
	isLODSelectionAutomatic_ = true;

	if (!HasCurrentMesh())
	{
		SetPreviewRenderActive(false);
		return;
	}

	const std::string meshPath = GetCurrentMeshPath();
	if (!meshPath.empty())
	{
		const size_t LODCount = GetLODCount();
		for (size_t LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			SetLODFrameCoverage(
				LODIndex,
				AssetParser::GetMeshLODFrameCoverage(meshPath, LODIndex, GetLODFrameCoverage(LODIndex)));
		}
	}

	ReloadCurrentLODMaterialPaths();

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

	if (viewportSize_.x != availableSize.x || viewportSize_.y != availableSize.y)
	{
		viewportSize_ = EditorUtils::ToVector2(availableSize);
		renderTarget_->SetFrameSize(viewportSize_);
	}

	UpdateCurrentLODFromFrameCoverage();

	if (!IsCurrentMeshReadyToView())
	{
		DrawEmptyViewportMessage(GetMeshNotReadyText());
		ImGui::EndChild();
		return;
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
	EditorUtils::DrawWorldAxis(cameraObject_->GetCameraComponent()->GetCamera());
	DrawViewportLODStats(cursorScreenPosition);
	const bool scrollSpeedControlHovered = DrawViewportScrollSpeedControl(cursorScreenPosition, availableSize);

	cameraObject_->GetController()->SetIsActive(viewportHovered && !scrollSpeedControlHovered);

	ImGui::EndChild();
}

void MeshAssetViewerPanelBase::DrawViewportLODStats(const ImVec2& viewportMin) const
{
	size_t vertexCount = 0;
	size_t faceCount = 0;
	const size_t subMeshCount = GetSubMeshCount();
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
	{
		vertexCount += GetSubMeshVertexCount(subMeshIndex);
		faceCount += GetSubMeshFaceCount(subMeshIndex);
	}

	const std::array<std::string, 4> lines =
	{
		"[LOD" + std::to_string(selectedLODIndex_) + "]",
		"Frame coverage: " + FormatFrameCoveragePercent(currentFrameCoverage_),
		"Vertex count: " + std::to_string(vertexCount),
		"Face count: " + std::to_string(faceCount)
	};

	ImVec2 textSize(0.f, 0.f);
	const float lineHeight = ImGui::GetTextLineHeight();
	for (const std::string& line : lines)
	{
		const ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
		textSize.x = std::max(textSize.x, lineSize.x);
		textSize.y += lineHeight;
	}

	const float padding = 8.f;
	const ImVec2 textMin(viewportMin.x + 12.f, viewportMin.y + 12.f);
	const ImVec2 backgroundMin(textMin.x - padding, textMin.y - padding);
	const ImVec2 backgroundMax(textMin.x + textSize.x + padding, textMin.y + textSize.y + padding);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(backgroundMin, backgroundMax, IM_COL32(10, 12, 16, 190), 4.f);

	ImVec2 textPosition = textMin;
	for (const std::string& line : lines)
	{
		drawList->AddText(textPosition, IM_COL32(235, 238, 242, 255), line.c_str());
		textPosition.y += lineHeight;
	}
}

bool MeshAssetViewerPanelBase::DrawViewportScrollSpeedControl(const ImVec2& viewportMin, const ImVec2& viewportSize)
{
	MeshViewerCameraController* controller = cameraObject_ ? cameraObject_->GetController() : nullptr;
	if (!controller || viewportSize.x < 220.f || viewportSize.y < 64.f)
	{
		return false;
	}

	const float outerPadding = 12.f;
	const float innerPadding = 8.f;
	const float controlWidth = 180.f;
	const float textHeight = ImGui::GetTextLineHeight();
	const float sliderHeight = ImGui::GetFrameHeight();
	const ImVec2 backgroundSize(controlWidth + innerPadding * 2.f, textHeight + sliderHeight + innerPadding * 3.f);
	const ImVec2 backgroundMin(viewportMin.x + viewportSize.x - backgroundSize.x - outerPadding, viewportMin.y + outerPadding);
	const ImVec2 backgroundMax(backgroundMin.x + backgroundSize.x, backgroundMin.y + backgroundSize.y);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(backgroundMin, backgroundMax, IM_COL32(10, 12, 16, 190), 4.f);

	ImGui::PushID("MeshViewerScrollSpeed");
	ImGui::SetCursorScreenPos(ImVec2(backgroundMin.x + innerPadding, backgroundMin.y + innerPadding));
	ImGui::TextUnformatted("Scroll Speed");

	float zoomSpeed = controller->GetZoomSpeed();
	ImGui::SetCursorScreenPos(ImVec2(backgroundMin.x + innerPadding, backgroundMin.y + innerPadding + textHeight + innerPadding));
	ImGui::SetNextItemWidth(controlWidth);
	if (ImGui::SliderFloat("##ScrollSpeed", &zoomSpeed, 0.05f, 20.f, "%.2f"))
	{
		controller->SetZoomSpeed(zoomSpeed);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Mouse wheel zoom speed");
	}

	ImGui::PopID();

	return ImGui::IsMouseHoveringRect(backgroundMin, backgroundMax);
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
	DrawLODSelector();
	ImGui::Text("Sub Meshes: %d", static_cast<int>(subMeshCount));

	ImGui::Spacing();
	ImGui::Separator();
	DrawLODSettings();
}

void MeshAssetViewerPanelBase::DrawLODSelector()
{
	const size_t LODCount = GetLODCount();
	if (LODCount == 0)
	{
		return;
	}

	ImGui::Text("LOD Selection");
	if (ImGui::Selectable("LOD Auto", isLODSelectionAutomatic_))
	{
		SelectAutomaticLOD();
	}

	for (size_t LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		ImGui::PushID(static_cast<int>(LODIndex));

		std::string label = "LOD " + std::to_string(LODIndex);
		if (isLODSelectionAutomatic_ && selectedLODIndex_ == LODIndex)
		{
			label += " (Current)";
		}

		if (ImGui::Selectable(label.c_str(), !isLODSelectionAutomatic_ && selectedLODIndex_ == LODIndex))
		{
			isLODSelectionAutomatic_ = false;
			SelectLOD(LODIndex, true);
		}

		ImGui::PopID();
	}

	ImGui::Spacing();
}

void MeshAssetViewerPanelBase::DrawLODSettings()
{
	const size_t LODCount = GetLODCount();
	if (LODCount == 0)
	{
		return;
	}

	ImGui::Text("LOD Settings");
	if (ImGui::Checkbox("Visualize Material Slots", &materialSlotVisualizerEnabled_))
	{
		RefreshPreviewMaterialOverrides();
	}

	const std::string meshPath = GetCurrentMeshPath();
	ImGui::PushID("LODSettings");
	for (size_t LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		ImGui::PushID(static_cast<int>(LODIndex));

		const std::string label = "LOD " + std::to_string(LODIndex);
		const ImGuiTreeNodeFlags flags = selectedLODIndex_ == LODIndex ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		if (ImGui::TreeNodeEx(label.c_str(), flags))
		{
			float frameCoveragePercent = GetFrameCoveragePercent(GetLODFrameCoverage(LODIndex));
			ImGui::Text("Frame Coverage:");
			ImGui::SameLine();
			if (LODIndex == 0)
			{
				ImGui::Text("%s", FormatFrameCoveragePercent(GetLODFrameCoverage(LODIndex)).c_str());
			}
			else if (ImGui::DragFloat("##FrameCoverage", &frameCoveragePercent, 0.1f, 0.f, 100.f, "%.2f%%"))
			{
				const float frameCoverage = std::max(0.f, std::min(100.f, frameCoveragePercent)) * 0.01f;
				SetLODFrameCoverage(LODIndex, frameCoverage);

				if (!meshPath.empty())
				{
					AssetParser::SetMeshLODFrameCoverage(meshPath, LODIndex, frameCoverage);
				}

				if (isLODSelectionAutomatic_)
				{
					UpdateCurrentLODFromFrameCoverage(true);
				}
			}

			const size_t subMeshCount = GetLODSubMeshCount(LODIndex);
			ImGui::Text("Sub Meshes: %d", static_cast<int>(subMeshCount));
			ImGui::Text("Materials");

			std::vector<std::string> materialPaths = ResolveMaterialPathsForSubMeshes(
				meshPath.empty() ? std::vector<std::string>{} : AssetParser::GetMeshLODMaterialPaths(meshPath, LODIndex),
				subMeshCount);

			for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
			{
				DrawLODMaterialSelector(LODIndex, subMeshIndex, materialPaths);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
	ImGui::PopID();
}

void MeshAssetViewerPanelBase::DrawLODMaterialSelector(size_t LODIndex, size_t subMeshIndex, std::vector<std::string>& materialPaths)
{
	ImGui::PushID(static_cast<int>(subMeshIndex));
	ImGui::Spacing();

	const std::string subMeshName = GetLODSubMeshName(LODIndex, subMeshIndex);
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

	materialPaths.resize(std::max(materialPaths.size(), subMeshIndex + 1));
	const std::string& materialPath = materialPaths[subMeshIndex];
	ImGui::TextWrapped("%s", materialPath.empty() ? "Grid default material (unset)" : materialPath.c_str());

	if (ImGui::Button("Select Asset"))
	{
		pendingMaterialSelectionLODIndex_ = static_cast<int>(LODIndex);
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

void MeshAssetViewerPanelBase::ReloadCurrentLODMaterialPaths()
{
	const std::string meshPath = GetCurrentMeshPath();
	selectedMaterialPaths_ = ResolveMaterialPathsForSubMeshes(
		meshPath.empty() ? std::vector<std::string>{} : AssetParser::GetMeshLODMaterialPaths(meshPath, selectedLODIndex_),
		GetSubMeshCount());
}

void MeshAssetViewerPanelBase::SelectAutomaticLOD()
{
	if (isLODSelectionAutomatic_)
	{
		return;
	}

	isLODSelectionAutomatic_ = true;
	UpdateCurrentLODFromFrameCoverage(true);
}

void MeshAssetViewerPanelBase::SelectLOD(size_t LODIndex, bool forceRefresh)
{
	if ((!forceRefresh && LODIndex == selectedLODIndex_) || GetLODCount() <= LODIndex)
	{
		return;
	}

	pendingMaterialSelectionLODIndex_ = -1;
	pendingMaterialSelectionSubMeshIndex_ = -1;
	selectedMaterialPaths_.clear();

	if (!SetCurrentLODIndex(LODIndex))
	{
		SetPreviewRenderActive(false);
		return;
	}

	selectedLODIndex_ = LODIndex;
	ReloadCurrentLODMaterialPaths();
	for (size_t subMeshIndex = 0; subMeshIndex < selectedMaterialPaths_.size(); ++subMeshIndex)
	{
		if (!selectedMaterialPaths_[subMeshIndex].empty())
		{
			RebuildCurrentMaterial(subMeshIndex, selectedMaterialPaths_[subMeshIndex]);
		}
	}

	InitializeCurrentMeshMaterials();
	RefreshPreviewMaterialOverrides();
	SetPreviewRenderActive(isOpen_);
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

	const int selectedLODIndex = pendingMaterialSelectionLODIndex_;
	const int selectedSubMeshIndex = pendingMaterialSelectionSubMeshIndex_;
	pendingMaterialSelectionLODIndex_ = -1;
	pendingMaterialSelectionSubMeshIndex_ = -1;

	if (!CanRenderCurrentMesh() ||
		selectedLODIndex < 0 ||
		selectedLODIndex >= static_cast<int>(GetLODCount()) ||
		selectedSubMeshIndex < 0 ||
		selectedSubMeshIndex >= static_cast<int>(GetLODSubMeshCount(static_cast<size_t>(selectedLODIndex))))
	{
		return;
	}

	const std::string relativeMaterialPath = EditorAssetPathUtils::ToContentRelativePath(path);
	if (relativeMaterialPath.empty() || !DoesMaterialAssetExist(relativeMaterialPath))
	{
		return;
	}

	if (!RebuildMaterial(static_cast<size_t>(selectedLODIndex), static_cast<size_t>(selectedSubMeshIndex), relativeMaterialPath))
	{
		return;
	}

	const std::string meshPath = GetCurrentMeshPath();
	std::vector<std::string> materialPaths = ResolveMaterialPathsForSubMeshes(
		meshPath.empty() ? std::vector<std::string>{} : AssetParser::GetMeshLODMaterialPaths(meshPath, static_cast<size_t>(selectedLODIndex)),
		GetLODSubMeshCount(static_cast<size_t>(selectedLODIndex)));
	materialPaths.resize(std::max(materialPaths.size(), static_cast<size_t>(selectedSubMeshIndex + 1)));
	materialPaths[selectedSubMeshIndex] = relativeMaterialPath;

	if (!meshPath.empty())
	{
		AssetParser::SetMeshLODMaterialPaths(meshPath, static_cast<size_t>(selectedLODIndex), materialPaths);
	}

	if (static_cast<size_t>(selectedLODIndex) == selectedLODIndex_)
	{
		selectedMaterialPaths_ = materialPaths;
		RefreshPreviewMaterialOverrides();
	}
}

void MeshAssetViewerPanelBase::UpdateCurrentLODFromFrameCoverage(bool forceRefresh)
{
	currentFrameCoverage_ = 0.f;

	Camera* camera = cameraObject_ ? cameraObject_->GetCameraComponent()->GetCamera() : nullptr;
	const Box* meshBounds = GetCurrentMeshCoverageBounds();
	const Matrix* meshTransformationMatrix = GetCurrentMeshWorldTransformationMatrix();
	if (!camera || !meshBounds || !meshTransformationMatrix)
	{
		return;
	}

	currentFrameCoverage_ = camera->GetAABBFrameCoverage(*meshBounds, *meshTransformationMatrix);

	if (!isLODSelectionAutomatic_)
	{
		return;
	}

	const size_t LODCount = GetLODCount();
	if (LODCount == 0)
	{
		return;
	}

	size_t LODIndex = GetLODIndexForFrameCoverage(currentFrameCoverage_);
	if (LODCount <= LODIndex)
	{
		LODIndex = LODCount - 1;
	}

	SelectLOD(LODIndex, forceRefresh);
}

bool MeshAssetViewerPanelBase::CanRenderCurrentMesh() const
{
	return HasCurrentMesh() && IsCurrentMeshReadyToView();
}
