#pragma once

#include "EditorPanel.h"

class AssetPickerPanel : public IEditorPanel
{
public:
	AssetPickerPanel(EditorHUD* hud);
	virtual void Draw() override;

private:
};