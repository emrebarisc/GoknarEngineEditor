#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "EditorPanel.h"

#include "Goknar/Delegates/Delegate.h"

struct Folder;

class AssetSelectorPanel : public IEditorPanel
{
public:
	AssetSelectorPanel(EditorHUD* hud);
	virtual void Draw() override;
	virtual void SetIsOpen(bool isOpen) override;

	static void SetMultiSelectionEnabled(bool enabled);

	inline static Delegate<void(const std::string&)> OnAssetSelected;
	inline static Delegate<void(const std::vector<std::string>&)> OnAssetsSelected;

private:
	void ResetMultiSelectionState();

	inline static bool isMultiSelectionEnabled_{ false };

	std::unordered_set<std::string> selectedAssetPaths_;
};
