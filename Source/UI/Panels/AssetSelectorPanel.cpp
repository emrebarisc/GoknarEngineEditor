#include "AssetSelectorPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorWidgets.h"

namespace
{
	const char* GetAssetTypeLabel(EditorAssetType assetType)
	{
		switch (assetType)
		{
		case EditorAssetType::Material: return "Material";
		case EditorAssetType::MaterialFunction: return "Material Function";
		case EditorAssetType::Texture: return "Texture";
		case EditorAssetType::StaticMesh: return "Static Mesh";
		case EditorAssetType::SkeletalMesh: return "Skeletal Mesh";
		case EditorAssetType::AnimationGraph: return "Animation Graph";
		case EditorAssetType::Audio: return "Audio";
		case EditorAssetType::Scene: return "Scene";
		case EditorAssetType::HeaderFile: return "Header";
		case EditorAssetType::SourceFile: return "Source";
		case EditorAssetType::Unknown: return "Unknown";
		case EditorAssetType::None:
		default:
			return "All";
		}
	}
}

AssetSelectorPanel::AssetSelectorPanel(EditorHUD* hud) : 
	IEditorPanel("Asset Selector", hud)
{
	isOpen_ = false;
}

void AssetSelectorPanel::Draw()
{
	const Vector2i& windowSize = EditorContext::Get()->windowSize;

	ImVec2 assetPickerWindowSize(600.f, 600.f);
	ImGui::SetNextWindowPos(ImVec2((windowSize.x - assetPickerWindowSize.x) * 0.5f, (windowSize.y - assetPickerWindowSize.y) * 0.5f));
	ImGui::SetNextWindowSize(assetPickerWindowSize);

	ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize);
	std::string selectedAssetPathFromGrid;
	bool isAFileSelected = false;
	const EditorAssetType filter = EditorContext::Get()->assetSelectorFilter;

	ImGui::Text("Filter: %s", GetAssetTypeLabel(filter));
	ImGui::Separator();

	EditorWidgets::DrawFileGrid(EditorContext::Get()->rootFolder, selectedAssetPathFromGrid, isAFileSelected, filter);
	
	if (isAFileSelected)
	{
		if (!OnAssetSelected.isNull())
		{
			OnAssetSelected(selectedAssetPathFromGrid);
			//OnAssetSelected = nullptr;
		}
		isOpen_ = false;
	}
	
	ImGui::End();
}
