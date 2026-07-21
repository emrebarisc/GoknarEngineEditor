#pragma once

#include "EditorPanel.h"

#include "imgui.h"

#include <memory>
#include <string>
#include <vector>

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
		panOffset_(0.0f, 0.0f),
		resizeWidth_(0),
		resizeHeight_(0),
		sourceImageWidth_(0),
		sourceImageHeight_(0),
		sourceImageChannels_(0),
		keepAspectRatio_(true),
		hasPendingResize_(false),
		shouldOpenOverwritePopup_(false),
		shouldOpenConversionOverwritePopup_(false)
	{
		isOpen_ = false;
	}
	~ImageViewerPanel() override;

	virtual void Draw() override;
	void SetTargetTexture(Texture* texture);
	void SetTargetImage(Image* image);

private:
	void ResetView();
	void ResetResizeState();
	bool LoadSourceImage();
	void RebuildDisplayTexture(const std::vector<unsigned char>& buffer, int width, int height);
	void DrawResizeControls();
	void DrawConversionControls();
	void DrawOverwriteConfirmationPopup();
	void DrawConversionOverwriteConfirmationPopup();
	void ApplyResizeInput(int newWidth, int newHeight);
	bool ApplyPendingResizeToPreview();
	bool SavePendingResize();
	void RequestImageConversion();
	bool ConvertImageToPath(const std::string& path);
	bool CanSavePendingResize() const;
	bool CanConvertImage() const;
	bool IsTargetImageWritable() const;
	std::string GetConvertedImagePath() const;
	std::string GetConversionButtonLabel() const;
	Texture* GetDisplayTexture() const;
	int GetDisplayWidth() const;
	int GetDisplayHeight() const;

	Image* targetImage_;
	Texture* targetTexture_;
	std::unique_ptr<Texture> displayTexture_;
	float zoom_;
	ImVec2 panOffset_;

	int resizeWidth_;
	int resizeHeight_;
	int sourceImageWidth_;
	int sourceImageHeight_;
	int sourceImageChannels_;
	bool keepAspectRatio_;
	bool hasPendingResize_;
	bool shouldOpenOverwritePopup_;
	bool shouldOpenConversionOverwritePopup_;
	std::vector<unsigned char> sourceImageBuffer_;
	std::vector<unsigned char> pendingResizeBuffer_;
	std::string resizeStatus_;
	std::string pendingConversionPath_;
};
