#pragma once

#include "EditorPanel.h"

#include "imgui.h"

class Texture;

class ImageViewerPanel : public IEditorPanel
{
public:
	ImageViewerPanel(EditorHUD* hud) :
		IEditorPanel("Image Viewer", hud),
		targetTexture_(nullptr),
		zoom_(1.0f),
		panOffset_(0.0f, 0.0f)
	{
		isOpen_ = false;
	}

	virtual void Draw() override;
	void SetTargetTexture(Texture* texture) { targetTexture_ = texture; }

private:
	Texture* targetTexture_;
	float zoom_;
	ImVec2 panOffset_;
};