#pragma once

#include <string>

#include "EditorPanel.h"

#include "Goknar/Delegates/Delegate.h"

struct Folder;

class AssetSelectorPanel : public IEditorPanel
{
public:
	AssetSelectorPanel(EditorHUD* hud);
	virtual void Draw() override;

	inline static Delegate<void(const std::string&)> OnAssetSelected;

private:
};