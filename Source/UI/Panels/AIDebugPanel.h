#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "UI/Panels/EditorPanel.h"

class AIDebugPanel : public IEditorPanel
{
public:
	AIDebugPanel(EditorHUD* hud);
	virtual void Draw() override;

private:
};
