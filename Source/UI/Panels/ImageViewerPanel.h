#pragma once

#include "EditorPanel.h"

#include "imgui.h"

class Texture;
class Image;

class ImageViewerPanel : public IEditorPanel
{
public:
	ImageViewerPanel(EditorHUD* hud) :
		IEditorPanel("Image Viewer", hud),
		targetImage_(nullptr),
		targetTexture_(nullptr),
		zoom_(1.0f),
		panOffset_(0.0f, 0.0f)
	{
		isOpen_ = false;
	}

	virtual void Draw() override;
	void SetTargetTexture(Texture* texture);
	void SetTargetImage(Image* image);

private:
	Image* targetImage_;
	Texture* targetTexture_;
	float zoom_;
	ImVec2 panOffset_;
};
