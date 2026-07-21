#include "ImageViewerPanel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "imgui.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/IO/IOManager.h"
#include "Goknar/Renderer/Texture.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace
{
	constexpr int MaxImageDimension = 16384;
	constexpr int JpegQuality = 90;
	constexpr size_t MaxResizeBufferBytes = 512ull * 1024ull * 1024ull;
	constexpr const char* OverwritePopupName = "Overwrite image?";
	constexpr const char* ConversionOverwritePopupName = "Overwrite converted image?";

	constexpr std::array<TextureUsage, 10> TextureUsageOptions =
	{
		TextureUsage::None,
		TextureUsage::Diffuse,
		TextureUsage::Normal,
		TextureUsage::AmbientOcclusion,
		TextureUsage::Metallic,
		TextureUsage::Specular,
		TextureUsage::Emissive,
		TextureUsage::Roughness,
		TextureUsage::Height,
		TextureUsage::ORM
	};

	ImVec2 GetTextureUV0(const Texture* texture)
	{
		return texture ? ImVec2(texture->GetAtlasUOffset(), texture->GetAtlasVOffset()) : ImVec2(0.0f, 0.0f);
	}

	ImVec2 GetTextureUV1(const Texture* texture)
	{
		return texture ?
			ImVec2(texture->GetAtlasUOffset() + texture->GetAtlasUScale(), texture->GetAtlasVOffset() + texture->GetAtlasVScale()) :
			ImVec2(1.0f, 1.0f);
	}

	int ClampImageDimension(int value)
	{
		return std::clamp(value, 1, MaxImageDimension);
	}

	bool TryGetImageBufferSize(int width, int height, int channels, size_t& bufferSize)
	{
		if (width <= 0 || height <= 0 || channels <= 0)
		{
			return false;
		}

		const size_t widthSize = static_cast<size_t>(width);
		const size_t heightSize = static_cast<size_t>(height);
		const size_t channelSize = static_cast<size_t>(channels);
		if (widthSize > MaxResizeBufferBytes / heightSize ||
			widthSize * heightSize > MaxResizeBufferBytes / channelSize)
		{
			return false;
		}

		bufferSize = widthSize * heightSize * channelSize;
		return bufferSize <= MaxResizeBufferBytes;
	}

	std::string ToLower(std::string value)
	{
		std::transform(
			value.begin(),
			value.end(),
			value.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return value;
	}

	bool IsPngPath(const std::string& path)
	{
		return ToLower(std::filesystem::path(path).extension().generic_string()) == ".png";
	}

	bool IsJpgPath(const std::string& path)
	{
		const std::string extension = ToLower(std::filesystem::path(path).extension().generic_string());
		return extension == ".jpg" || extension == ".jpeg";
	}

	bool IsWritableImagePath(const std::string& path)
	{
		return IsPngPath(path) || IsJpgPath(path);
	}

	bool WriteResizedImage(const std::string& path, int width, int height, int channels, const unsigned char* buffer)
	{
		if (IsPngPath(path))
		{
			return IOManager::WritePng(path.c_str(), width, height, channels, buffer);
		}

		if (IsJpgPath(path))
		{
			return stbi_write_jpg(path.c_str(), width, height, channels, buffer, JpegQuality) == 1;
		}

		return false;
	}

	std::string GetPathFilename(const std::string& path)
	{
		return std::filesystem::path(path).filename().generic_string();
	}

	void ApplyTextureFormatForChannels(Texture* texture, int channels)
	{
		if (!texture)
		{
			return;
		}

		if (channels == 1)
		{
			texture->SetTextureFormat(TextureFormat::RED);
			texture->SetTextureInternalFormat(TextureInternalFormat::RED);
		}
		else if (channels == 2)
		{
			texture->SetTextureFormat(TextureFormat::RG);
			texture->SetTextureInternalFormat(TextureInternalFormat::RG);
		}
		else if (channels == 4)
		{
			texture->SetTextureFormat(TextureFormat::RGBA);
			texture->SetTextureInternalFormat(TextureInternalFormat::RGBA);
		}
		else
		{
			texture->SetTextureFormat(TextureFormat::RGB);
			texture->SetTextureInternalFormat(TextureInternalFormat::RGB);
		}
	}

	std::vector<unsigned char> ResizeImageBilinear(
		const std::vector<unsigned char>& source,
		int sourceWidth,
		int sourceHeight,
		int channels,
		int targetWidth,
		int targetHeight)
	{
		size_t targetBufferSize = 0;
		if (source.empty() ||
			!TryGetImageBufferSize(targetWidth, targetHeight, channels, targetBufferSize))
		{
			return {};
		}

		if (sourceWidth == targetWidth && sourceHeight == targetHeight)
		{
			return source;
		}

		std::vector<unsigned char> target(targetBufferSize);
		const float xScale = targetWidth > 1 ? static_cast<float>(sourceWidth - 1) / static_cast<float>(targetWidth - 1) : 0.0f;
		const float yScale = targetHeight > 1 ? static_cast<float>(sourceHeight - 1) / static_cast<float>(targetHeight - 1) : 0.0f;

		for (int y = 0; y < targetHeight; ++y)
		{
			const float sourceY = static_cast<float>(y) * yScale;
			const int y0 = std::clamp(static_cast<int>(sourceY), 0, sourceHeight - 1);
			const int y1 = std::min(y0 + 1, sourceHeight - 1);
			const float yWeight = sourceY - static_cast<float>(y0);

			for (int x = 0; x < targetWidth; ++x)
			{
				const float sourceX = static_cast<float>(x) * xScale;
				const int x0 = std::clamp(static_cast<int>(sourceX), 0, sourceWidth - 1);
				const int x1 = std::min(x0 + 1, sourceWidth - 1);
				const float xWeight = sourceX - static_cast<float>(x0);

				for (int channel = 0; channel < channels; ++channel)
				{
					const size_t topLeftIndex = (static_cast<size_t>(y0) * sourceWidth + x0) * channels + channel;
					const size_t topRightIndex = (static_cast<size_t>(y0) * sourceWidth + x1) * channels + channel;
					const size_t bottomLeftIndex = (static_cast<size_t>(y1) * sourceWidth + x0) * channels + channel;
					const size_t bottomRightIndex = (static_cast<size_t>(y1) * sourceWidth + x1) * channels + channel;

					const float top = source[topLeftIndex] + (source[topRightIndex] - source[topLeftIndex]) * xWeight;
					const float bottom = source[bottomLeftIndex] + (source[bottomRightIndex] - source[bottomLeftIndex]) * xWeight;
					const float value = top + (bottom - top) * yWeight;

					const size_t targetIndex = (static_cast<size_t>(y) * targetWidth + x) * channels + channel;
					target[targetIndex] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
				}
			}
		}

		return target;
	}
}

ImageViewerPanel::~ImageViewerPanel() = default;

void ImageViewerPanel::SetTargetTexture(Texture* texture)
{
	targetImage_ = nullptr;
	targetTexture_ = texture;
	ResetView();
	ResetResizeState();
	if (targetTexture_)
	{
		resizeWidth_ = targetTexture_->GetWidth();
		resizeHeight_ = targetTexture_->GetHeight();
	}
}

void ImageViewerPanel::SetTargetImage(Image* image)
{
	targetImage_ = image;
	targetTexture_ = image ? image->GetOrCreateGeneratedTexture() : nullptr;
	ResetView();
	ResetResizeState();

	if (targetImage_ && !LoadSourceImage())
	{
		resizeStatus_ = "Could not load image pixels for resizing.";
	}
}

void ImageViewerPanel::ResetView()
{
	zoom_ = 1.0f;
	panOffset_ = ImVec2(0.0f, 0.0f);
}

void ImageViewerPanel::ResetResizeState()
{
	displayTexture_.reset();
	sourceImageBuffer_.clear();
	pendingResizeBuffer_.clear();
	resizeStatus_.clear();
	resizeWidth_ = 0;
	resizeHeight_ = 0;
	sourceImageWidth_ = 0;
	sourceImageHeight_ = 0;
	sourceImageChannels_ = 0;
	keepAspectRatio_ = true;
	hasPendingResize_ = false;
	shouldOpenOverwritePopup_ = false;
	shouldOpenConversionOverwritePopup_ = false;
	pendingConversionPath_.clear();
}

bool ImageViewerPanel::LoadSourceImage()
{
	if (!targetImage_)
	{
		return false;
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	const unsigned char* rawBuffer = nullptr;
	if (!IOManager::ReadImage(targetImage_->GetPath().c_str(), width, height, channels, &rawBuffer) || !rawBuffer)
	{
		return false;
	}

	size_t bufferSize = 0;
	if (!TryGetImageBufferSize(width, height, channels, bufferSize))
	{
		delete[] const_cast<unsigned char*>(rawBuffer);
		return false;
	}

	sourceImageBuffer_.assign(rawBuffer, rawBuffer + bufferSize);
	delete[] const_cast<unsigned char*>(rawBuffer);

	sourceImageWidth_ = width;
	sourceImageHeight_ = height;
	sourceImageChannels_ = channels;
	resizeWidth_ = width;
	resizeHeight_ = height;

	RebuildDisplayTexture(sourceImageBuffer_, sourceImageWidth_, sourceImageHeight_);

	if (!IsTargetImageWritable())
	{
		resizeStatus_ = "Save is available for PNG and JPG assets.";
	}

	return true;
}

void ImageViewerPanel::RebuildDisplayTexture(const std::vector<unsigned char>& buffer, int width, int height)
{
	displayTexture_.reset();

	size_t bufferSize = 0;
	if (buffer.empty() || !TryGetImageBufferSize(width, height, sourceImageChannels_, bufferSize))
	{
		return;
	}

	unsigned char* textureBuffer = new unsigned char[bufferSize];
	std::memcpy(textureBuffer, buffer.data(), bufferSize);

	std::unique_ptr<Texture> texture = std::make_unique<Texture>();
	texture->SetSize(width, height);
	texture->SetChannels(sourceImageChannels_);
	texture->SetBuffer(textureBuffer);
	texture->SetTextureUsage(targetImage_ ? targetImage_->GetTextureUsage() : TextureUsage::Diffuse);
	ApplyTextureFormatForChannels(texture.get(), sourceImageChannels_);

	if (targetImage_)
	{
		const std::string textureName = std::filesystem::path(targetImage_->GetPath()).filename().generic_string();
		texture->SetName(textureName);
	}

	texture->PreInit();
	texture->Init();
	texture->PostInit();

	displayTexture_ = std::move(texture);
}

void ImageViewerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	Texture* displayTexture = GetDisplayTexture();
	if (!displayTexture)
	{
		ImGui::TextDisabled("No image selected");
		ImGui::End();
		return;
	}

	if (ImGui::BeginTable("ImageViewerSplitLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("ImageViewport", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGuiIO& io = ImGui::GetIO();

		// --- Handle Zoom ---
		if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
		{
			float zoomDelta = io.MouseWheel * 0.1f;
			zoom_ += zoomDelta;
			if (zoom_ < 0.1f) zoom_ = 0.1f; // Cap minimum zoom
		}

		// --- Handle Panning (Middle Mouse Button) ---
		if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			panOffset_.x += io.MouseDelta.x;
			panOffset_.y += io.MouseDelta.y;
		}

		// --- Calculate Dimensions ---
		float tw = (float)displayTexture->GetWidth();
		float th = (float)displayTexture->GetHeight();

		if (tw > 0.0f && th > 0.0f)
		{
			// Base scale to fit window initially
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float baseScale = std::min(avail.x / tw, avail.y / th);
			baseScale = std::max(baseScale, 0.01f);

			ImVec2 displaySize(tw * baseScale * zoom_, th * baseScale * zoom_);

			// --- Rendering ---
			// Set cursor to the panned position (relative to window start)
			ImVec2 origin = ImGui::GetCursorStartPos();
			ImGui::SetCursorPos(ImVec2(origin.x + panOffset_.x, origin.y + panOffset_.y));

			ImGui::Image(
				(ImTextureID)(intptr_t)displayTexture->GetRendererTextureId(),
				displaySize,
				GetTextureUV0(displayTexture),
				GetTextureUV1(displayTexture));
		}

		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("ImageProperties");

		if (targetImage_)
		{
			const std::string textureName = std::filesystem::path(targetImage_->GetPath()).filename().generic_string();
			ImGui::TextWrapped("%s", textureName.c_str());
		}

		ImGui::Text("%d x %d", GetDisplayWidth(), GetDisplayHeight());
		ImGui::Separator();

		TextureUsage currentUsage = targetImage_ ? targetImage_->GetTextureUsage() : targetTexture_->GetTextureUsage();
		if (ImGui::BeginCombo("Texture Usage", AssetParser::TextureUsageToString(currentUsage)))
		{
			for (TextureUsage textureUsage : TextureUsageOptions)
			{
				const bool isSelected = currentUsage == textureUsage;
				if (ImGui::Selectable(AssetParser::TextureUsageToString(textureUsage), isSelected))
				{
					currentUsage = textureUsage;
					if (targetImage_)
					{
						targetImage_->SetTextureUsage(textureUsage);
						AssetParser::SetTextureUsage(targetImage_->GetPath(), textureUsage);
					}
					if (targetTexture_)
					{
						targetTexture_->SetTextureUsage(textureUsage);
					}
					if (displayTexture_)
					{
						displayTexture_->SetTextureUsage(textureUsage);
					}
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		DrawResizeControls();
		DrawConversionControls();

		if (ImGui::Button("Reset View"))
		{
			ResetView();
		}

		ImGui::EndChild();
		ImGui::EndTable();
	}

	DrawOverwriteConfirmationPopup();
	DrawConversionOverwriteConfirmationPopup();

	ImGui::End();
}

void ImageViewerPanel::DrawResizeControls()
{
	ImGui::Separator();
	ImGui::TextUnformatted("Resize");

	const bool canResize = targetImage_ && !sourceImageBuffer_.empty();
	if (!canResize)
	{
		ImGui::TextDisabled("Open an image asset to resize.");
	}

	ImGui::BeginDisabled(!canResize);

	const bool aspectRatioChanged = ImGui::Checkbox("Keep Ratio", &keepAspectRatio_);

	int newWidth = resizeWidth_;
	int newHeight = resizeHeight_;

	ImGui::PushItemWidth(100.0f);
	const bool widthChanged = ImGui::InputInt("Width", &newWidth, 1, 16);
	const bool heightChanged = ImGui::InputInt("Height", &newHeight, 1, 16);
	ImGui::PopItemWidth();

	newWidth = ClampImageDimension(newWidth);
	newHeight = ClampImageDimension(newHeight);

	if (canResize && keepAspectRatio_ && sourceImageWidth_ > 0 && sourceImageHeight_ > 0)
	{
		if (widthChanged && !heightChanged)
		{
			newHeight = ClampImageDimension(static_cast<int>(std::lround(
				static_cast<double>(newWidth) * static_cast<double>(sourceImageHeight_) / static_cast<double>(sourceImageWidth_))));
		}
		else if (heightChanged)
		{
			newWidth = ClampImageDimension(static_cast<int>(std::lround(
				static_cast<double>(newHeight) * static_cast<double>(sourceImageWidth_) / static_cast<double>(sourceImageHeight_))));
		}
		else if (aspectRatioChanged)
		{
			newHeight = ClampImageDimension(static_cast<int>(std::lround(
				static_cast<double>(newWidth) * static_cast<double>(sourceImageHeight_) / static_cast<double>(sourceImageWidth_))));
		}
	}

	if (canResize && (widthChanged || heightChanged || aspectRatioChanged))
	{
		ApplyResizeInput(newWidth, newHeight);
	}

	ImGui::EndDisabled();

	ImGui::BeginDisabled(!CanSavePendingResize());
	if (ImGui::Button("Save"))
	{
		shouldOpenOverwritePopup_ = true;
	}
	ImGui::EndDisabled();

	if (!resizeStatus_.empty())
	{
		ImGui::TextWrapped("%s", resizeStatus_.c_str());
	}
}

void ImageViewerPanel::DrawConversionControls()
{
	ImGui::Separator();
	ImGui::TextUnformatted("Convert");

	const bool canConvert = CanConvertImage();
	const std::string convertedImagePath = canConvert ? GetConvertedImagePath() : std::string();
	if (!canConvert)
	{
		ImGui::TextDisabled("PNG and JPG images can be converted.");
	}
	else
	{
		ImGui::TextWrapped("%s", GetPathFilename(convertedImagePath).c_str());
	}

	ImGui::BeginDisabled(!canConvert);
	const std::string conversionButtonLabel = GetConversionButtonLabel();
	if (ImGui::Button(conversionButtonLabel.c_str()))
	{
		RequestImageConversion();
	}
	ImGui::EndDisabled();
}

void ImageViewerPanel::DrawOverwriteConfirmationPopup()
{
	if (shouldOpenOverwritePopup_)
	{
		ImGui::OpenPopup(OverwritePopupName);
		shouldOpenOverwritePopup_ = false;
	}

	if (ImGui::BeginPopupModal(OverwritePopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Are you sure you want to override the existing image?");

		if (ImGui::Button("Override", ImVec2(120.0f, 0.0f)))
		{
			SavePendingResize();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void ImageViewerPanel::DrawConversionOverwriteConfirmationPopup()
{
	if (shouldOpenConversionOverwritePopup_)
	{
		ImGui::OpenPopup(ConversionOverwritePopupName);
		shouldOpenConversionOverwritePopup_ = false;
	}

	if (ImGui::BeginPopupModal(ConversionOverwritePopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped(
			"A file named %s already exists. Override it?",
			GetPathFilename(pendingConversionPath_).c_str());

		if (ImGui::Button("Override", ImVec2(120.0f, 0.0f)))
		{
			ConvertImageToPath(pendingConversionPath_);
			pendingConversionPath_.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		{
			pendingConversionPath_.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void ImageViewerPanel::ApplyResizeInput(int newWidth, int newHeight)
{
	resizeWidth_ = ClampImageDimension(newWidth);
	resizeHeight_ = ClampImageDimension(newHeight);

	if (resizeWidth_ == sourceImageWidth_ && resizeHeight_ == sourceImageHeight_)
	{
		pendingResizeBuffer_.clear();
		hasPendingResize_ = false;
		RebuildDisplayTexture(sourceImageBuffer_, sourceImageWidth_, sourceImageHeight_);
		resizeStatus_ = IsTargetImageWritable() ? std::string() : "Save is available for PNG and JPG assets.";
		return;
	}

	if (!ApplyPendingResizeToPreview())
	{
		resizeStatus_ = "Could not resize image with the requested dimensions.";
		return;
	}

	resizeStatus_ = IsTargetImageWritable() ? "Resize pending." : "Preview resized. Save is available for PNG and JPG assets.";
}

bool ImageViewerPanel::ApplyPendingResizeToPreview()
{
	pendingResizeBuffer_ = ResizeImageBilinear(
		sourceImageBuffer_,
		sourceImageWidth_,
		sourceImageHeight_,
		sourceImageChannels_,
		resizeWidth_,
		resizeHeight_);

	if (pendingResizeBuffer_.empty())
	{
		hasPendingResize_ = false;
		return false;
	}

	hasPendingResize_ = true;
	RebuildDisplayTexture(pendingResizeBuffer_, resizeWidth_, resizeHeight_);
	return true;
}

bool ImageViewerPanel::SavePendingResize()
{
	if (!CanSavePendingResize())
	{
		return false;
	}

	if (!WriteResizedImage(
		targetImage_->GetPath().c_str(),
		resizeWidth_,
		resizeHeight_,
		sourceImageChannels_,
		pendingResizeBuffer_.data()))
	{
		resizeStatus_ = "Failed to save resized image.";
		return false;
	}

	sourceImageBuffer_ = pendingResizeBuffer_;
	sourceImageWidth_ = resizeWidth_;
	sourceImageHeight_ = resizeHeight_;
	pendingResizeBuffer_.clear();
	hasPendingResize_ = false;
	RebuildDisplayTexture(sourceImageBuffer_, sourceImageWidth_, sourceImageHeight_);
	resizeStatus_ = "Saved resized image.";
	return true;
}

void ImageViewerPanel::RequestImageConversion()
{
	pendingConversionPath_ = GetConvertedImagePath();
	if (pendingConversionPath_.empty())
	{
		resizeStatus_ = "Could not resolve converted image path.";
		return;
	}

	std::error_code errorCode;
	if (std::filesystem::exists(pendingConversionPath_, errorCode))
	{
		shouldOpenConversionOverwritePopup_ = true;
		return;
	}

	ConvertImageToPath(pendingConversionPath_);
	pendingConversionPath_.clear();
}

bool ImageViewerPanel::ConvertImageToPath(const std::string& path)
{
	if (!CanConvertImage())
	{
		return false;
	}

	const bool usePendingResize = hasPendingResize_ && !pendingResizeBuffer_.empty();
	const std::vector<unsigned char>& buffer = usePendingResize ? pendingResizeBuffer_ : sourceImageBuffer_;
	const int width = usePendingResize ? resizeWidth_ : sourceImageWidth_;
	const int height = usePendingResize ? resizeHeight_ : sourceImageHeight_;

	if (!WriteResizedImage(path, width, height, sourceImageChannels_, buffer.data()))
	{
		resizeStatus_ = "Failed to convert image.";
		return false;
	}

	resizeStatus_ = "Converted image to " + GetPathFilename(path) + ".";
	return true;
}

bool ImageViewerPanel::CanSavePendingResize() const
{
	return hasPendingResize_ &&
		targetImage_ &&
		IsTargetImageWritable() &&
		!pendingResizeBuffer_.empty();
}

bool ImageViewerPanel::CanConvertImage() const
{
	if (!targetImage_ || sourceImageBuffer_.empty())
	{
		return false;
	}

	const std::string& path = targetImage_->GetPath();
	return IsPngPath(path) || IsJpgPath(path);
}

bool ImageViewerPanel::IsTargetImageWritable() const
{
	return targetImage_ && IsWritableImagePath(targetImage_->GetPath());
}

std::string ImageViewerPanel::GetConvertedImagePath() const
{
	if (!targetImage_)
	{
		return "";
	}

	std::filesystem::path convertedPath(targetImage_->GetPath());
	if (IsPngPath(targetImage_->GetPath()))
	{
		convertedPath.replace_extension(".jpg");
	}
	else if (IsJpgPath(targetImage_->GetPath()))
	{
		convertedPath.replace_extension(".png");
	}
	else
	{
		return "";
	}

	return convertedPath.generic_string();
}

std::string ImageViewerPanel::GetConversionButtonLabel() const
{
	if (!targetImage_)
	{
		return "Convert";
	}

	if (IsPngPath(targetImage_->GetPath()))
	{
		return "Convert to JPG";
	}

	if (IsJpgPath(targetImage_->GetPath()))
	{
		return "Convert to PNG";
	}

	return "Convert";
}

Texture* ImageViewerPanel::GetDisplayTexture() const
{
	return displayTexture_ ? displayTexture_.get() : targetTexture_;
}

int ImageViewerPanel::GetDisplayWidth() const
{
	const Texture* displayTexture = GetDisplayTexture();
	return displayTexture ? displayTexture->GetWidth() : 0;
}

int ImageViewerPanel::GetDisplayHeight() const
{
	const Texture* displayTexture = GetDisplayTexture();
	return displayTexture ? displayTexture->GetHeight() : 0;
}
