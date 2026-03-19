#pragma once

#include "EditorPanel.h"

class DebugPanel : public IEditorPanel
{
public:
	DebugPanel(EditorHUD* hud);
	virtual void Draw() override;
	void DrawOverlay(const Vector2& viewportPos, const Vector2& viewportSize);

private:
};