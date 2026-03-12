#include "ImageViewerPanel.h"
#include "imgui.h"
#include "Goknar/Renderer/Texture.h"

void ImageViewerPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);
	if (targetTexture_)
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float tw = (float)targetTexture_->GetWidth();
		float th = (float)targetTexture_->GetHeight();

		float scale = std::min(avail.x / tw, avail.y / th);
		ImVec2 displaySize(tw * scale, th * scale);

		ImGui::Image((ImTextureID)(intptr_t)targetTexture_->GetRendererTextureId(), displaySize);
	}
	ImGui::End();
}