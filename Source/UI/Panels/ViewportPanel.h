#pragma once

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"
#include "DebugPanel.h"

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorHUD* hud);
	virtual ~ViewportPanel();

	virtual void Init() override;
	virtual void Draw() override;

	bool GetDebugOverlayEnabled() const
	{
		return showDebugOverlay_;
	}

	void SetDebugOverlayEnabled(bool enabled) 
	{
		showDebugOverlay_ = enabled;
	}

private:
	Vector2 size_;
	Vector2 position_;
	DebugPanel* debugPanel_{ nullptr };
	bool showDebugOverlay_{ true };
};