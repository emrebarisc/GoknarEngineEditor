#include "AssetSelectorPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorWidgets.h"

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

	EditorWidgets::DrawFileGrid(EditorContext::Get()->rootFolder, selectedAssetPathFromGrid, isAFileSelected);
	
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