#pragma once

#include "EditorPanel.h"

class DebugPanel : public IEditorPanel
{
public:
	DebugPanel(EditorHUD* hud);
	virtual void Draw() override;

private:
};