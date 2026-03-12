#include "ImageViewerPanel.h"
#include "imgui.h"
#include "Goknar/Renderer/Texture.h"

void ImageViewerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	if (targetTexture_)
	{
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

		// Base scale to fit window initially
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float baseScale = std::min(avail.x / tw, avail.y / th);

		ImVec2 displaySize(tw * baseScale * zoom_, th * baseScale * zoom_);

		// --- Rendering ---
		// Set cursor to the panned position (relative to window start)
		ImVec2 origin = ImGui::GetCursorStartPos();
		ImGui::SetCursorPos(ImVec2(origin.x + panOffset_.x, origin.y + panOffset_.y));

		ImGui::Image((ImTextureID)(intptr_t)targetTexture_->GetRendererTextureId(), displaySize);
	}

	ImGui::End();
}