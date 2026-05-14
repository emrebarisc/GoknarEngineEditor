#include "ImageViewerPanel.h"

#include <algorithm>
#include <array>
#include <filesystem>

#include "imgui.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Helpers/AssetParser.h"
#include "Goknar/Renderer/Texture.h"

namespace
{
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
}

void ImageViewerPanel::SetTargetTexture(Texture* texture)
{
	targetImage_ = nullptr;
	targetTexture_ = texture;
	zoom_ = 1.0f;
	panOffset_ = ImVec2(0.0f, 0.0f);
}

void ImageViewerPanel::SetTargetImage(Image* image)
{
	targetImage_ = image;
	targetTexture_ = image ? image->GetOrCreateGeneratedTexture() : nullptr;
	zoom_ = 1.0f;
	panOffset_ = ImVec2(0.0f, 0.0f);
}

void ImageViewerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	if (!targetTexture_)
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
		float tw = (float)targetTexture_->GetWidth();
		float th = (float)targetTexture_->GetHeight();

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
				(ImTextureID)(intptr_t)targetTexture_->GetRendererTextureId(),
				displaySize,
				GetTextureUV0(targetTexture_),
				GetTextureUV1(targetTexture_));
		}

		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("ImageProperties");

		if (targetImage_)
		{
			const std::string textureName = std::filesystem::path(targetImage_->GetPath()).filename().generic_string();
			ImGui::TextWrapped("%s", textureName.c_str());
		}

		ImGui::Text("%d x %d", targetTexture_->GetWidth(), targetTexture_->GetHeight());
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
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("Reset View"))
		{
			zoom_ = 1.0f;
			panOffset_ = ImVec2(0.0f, 0.0f);
		}

		ImGui::EndChild();
		ImGui::EndTable();
	}

	ImGui::End();
}
