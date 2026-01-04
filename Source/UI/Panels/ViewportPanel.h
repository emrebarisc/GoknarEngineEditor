#pragma once

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorHUD* hud);
	virtual ~ViewportPanel();

	virtual void Init() override;
	virtual void Draw() override;

private:
};