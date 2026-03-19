#include "ToolBarPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorUtils.h"

#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"

void ToolBarPanel::Draw()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	float toolbarHeight = 36.0f;

	// Force the position exactly at WorkPos (which starts just below the MenuBar)
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
	ImGui::SetNextWindowViewport(viewport->ID);

	// Flags to make it a rigid, non-dockable bar
	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));

	if (ImGui::Begin("##ToolBar", nullptr, window_flags))
	{
		Image* uiImage = EditorUtils::GetEditorContent<Image>("Textures/UI/T_UI.png");
		if (uiImage && uiImage->GetGeneratedTexture())
		{
			ImTextureID atlasID = (ImTextureID)(intptr_t)uiImage->GetGeneratedTexture()->GetRendererTextureId();

			const float atlasSize = 1024.0f;
			const float spriteSize = 128.0f;

			auto GetUV0 = [&](float x, float y) { return ImVec2(x / atlasSize, y / atlasSize); };
			auto GetUV1 = [&](float x, float y) { return ImVec2((x + spriteSize) / atlasSize, (y + spriteSize) / atlasSize); };

			// Compile Icon (Gear/Code): Column 6, Row 7
			ImVec2 compileUv0 = GetUV0(768.0f, 896.0f);
			ImVec2 compileUv1 = GetUV1(768.0f, 896.0f);

			// Play Icon (Triangle): Column 7, Row 7
			ImVec2 playUv0 = GetUV0(896.0f, 896.0f);
			ImVec2 playUv1 = GetUV1(896.0f, 896.0f);

			float iconSize = 24.0f;

			if (ImGui::ImageButton("##CompileButton", atlasID, ImVec2(iconSize, iconSize), compileUv0, compileUv1))
			{
				// TODO: Hook up compilation logic
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Compile Current Project");

			ImGui::SameLine();

			if (ImGui::ImageButton("##PlayButton", atlasID, ImVec2(iconSize, iconSize), playUv0, playUv1))
			{
				// TODO: Hook up play logic
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play Current Project");
		}
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}