#pragma once

#include "EditorPanel.h"

class Texture;

class ImageViewerPanel : public IEditorPanel
{
public:
	ImageViewerPanel(EditorHUD* hud) : 
		IEditorPanel("Image Viewer", hud),
		targetTexture_(nullptr)
	{
		isOpen_ = false;
	}
	
	virtual void Draw() override;
	void SetTargetTexture(Texture* texture) { targetTexture_ = texture; }

private:
	Texture* targetTexture_;
};