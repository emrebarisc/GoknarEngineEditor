#include "ToolBarPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"
#include "UI/EditorUtils.h"

#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/ConfigManager.h"
#include "Goknar/Renderer/Texture.h"

void ToolBarPanel::Draw()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float menuBarHeight = ImGui::GetFrameHeight();
	const float toolbarHeight = GetToolbarHeight();

	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbarHeight));
	ImGui::SetNextWindowViewport(viewport->ID);

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

			ImVec2 compileUv0 = GetUV0(768.0f, 896.0f);
			ImVec2 compileUv1 = GetUV1(768.0f, 896.0f);

			ImVec2 playUv0 = GetUV0(896.0f, 896.0f);
			ImVec2 playUv1 = GetUV1(896.0f, 896.0f);

			float iconSize = 32.0f;
			float paddingBetween = 16.0f;

			ImGui::SetCursorPosX((viewport->Size.x - iconSize - paddingBetween) * 0.5f);
			if (ImGui::ImageButton("##CompileButton", atlasID, ImVec2(iconSize, iconSize), compileUv0, compileUv1))
			{
				std::string command = "cd " + ProjectDir.substr(0, ProjectDir.size() - 1) + " && Build.sh debug";

				asyncCompileResult = std::async(std::launch::async,
					[command]()
					{
						std::system(command.c_str());
					});
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Compile Project");
			}

			ImGui::SameLine();

			ImGui::SetCursorPosX((viewport->Size.x + iconSize + paddingBetween) * 0.5f);
			if (ImGui::ImageButton("##PlayButton", atlasID, ImVec2(iconSize, iconSize), playUv0, playUv1))
			{
				ConfigManager editorConfig;
				std::string currentProjectName = "";
				if (editorConfig.ReadFile("Config/EditorConfig.ini"))
				{
					currentProjectName = editorConfig.GetString("Editor", "CurrentProject", "");
				}

				std::string command = "cd " + ProjectDir + " && Build.sh onlySync &&";
#if GOKNAR_PLATFORM_WINDOWS
				command += "cd Build_Debug/Output/ && " + currentProjectName + ".exe";
#else
				command = "cd Build_Debug/Output/ && ./" + currentProjectName;
#endif
				asyncCompileResult = std::async(std::launch::async,
					[command]()
					{
						std::system(command.c_str());
					});
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Play Project");
			}
		}
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}
