#pragma once

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"

class FrameBuffer;
class Image;
class FreeCameraObject;

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorHUD* hud);
	virtual ~ViewportPanel();

	virtual void Init() override;
	virtual void Draw() override;

private:
	FrameBuffer* frameBuffer_{ nullptr };
	Image* viewportImage_{ nullptr};
	FreeCameraObject* cameraObject_{ nullptr};
	
	Vector2 viewportSize_;
	Vector2 viewportPos_;
	bool isFocused_;
};