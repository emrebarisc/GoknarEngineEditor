#pragma once

#include "EditorPanel.h"

class DebugPanel : public IEditorPanel
{
public:
	DebugPanel(EditorHUD* hud);
	virtual void Draw() override;
	void DrawOverlay(const Vector2i& viewportPos, const Vector2i& viewportSize);

private:
};