#include "EditorSettingsPanel.h"

#include <algorithm>
#include <cfloat>

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Renderer/Renderer.h"
#include "Goknar/Renderer/PostProcessing/GammaCorrectionPostProcessingEffect.h"
#include "UI/EditorConfigUtils.h"
#include "UI/EditorGameProjectBuildUtils.h"

EditorSettingsPanel::EditorSettingsPanel(EditorHUD* hud) :
	IEditorPanel("Editor Settings", hud)
{
	isOpen_ = false;
}

void EditorSettingsPanel::Init()
{
	LoadSettings();
}

void EditorSettingsPanel::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(640.f, 360.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	position_ = Vector2i((int)ImGui::GetWindowPos().x, (int)ImGui::GetWindowPos().y);
	size_ = Vector2i((int)ImGui::GetWindowSize().x, (int)ImGui::GetWindowSize().y);

	if (!statusMessage_.empty())
	{
		const ImVec4 statusColor = statusMessageIsError_ ? ImVec4(0.95f, 0.35f, 0.35f, 1.f) : ImVec4(0.35f, 0.85f, 0.45f, 1.f);
		ImGui::TextColored(statusColor, "%s", statusMessage_.c_str());
		ImGui::Separator();
	}

	ImGui::TextWrapped("%s", EditorGameProjectBuildUtils::GetEditorConfigPath().c_str());
	ImGui::Spacing();
	ImGui::Text("%s", "Rendering");
	ImGui::Separator();

	if (ImGui::BeginTable("EditorSettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", "Gamma Correction");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);

		float editedGammaCorrection = gammaCorrection_;
		if (ImGui::DragFloat("##GammaCorrection", &editedGammaCorrection, 0.01f, 0.1f, 5.f, "%.2f"))
		{
			gammaCorrection_ = std::clamp(editedGammaCorrection, 0.1f, 5.f);
			ApplyGammaCorrection();

			statusMessage_.clear();
			statusMessageIsError_ = false;
		}

		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			SaveGammaCorrection();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void EditorSettingsPanel::LoadSettings()
{
	gammaCorrection_ = EditorConfigUtils::GetGammaCorrection(EditorConfigUtils::DefaultGammaCorrection);
	gammaCorrection_ = std::clamp(gammaCorrection_, 0.1f, 5.f);
	ApplyGammaCorrection();
}

bool EditorSettingsPanel::ApplyGammaCorrection()
{
	if (!engine || !engine->GetRenderer())
	{
		return false;
	}

	GammaCorrectionPostProcessingEffect* gammaCorrectionPostProcessingEffect = engine->GetRenderer()->GetGammaCorrectionPostProcessingEffect();
	if (!gammaCorrectionPostProcessingEffect)
	{
		return false;
	}

	gammaCorrectionPostProcessingEffect->SetGamma(gammaCorrection_);
	return true;
}

void EditorSettingsPanel::SaveGammaCorrection()
{
	if (EditorConfigUtils::SetGammaCorrection(gammaCorrection_))
	{
		statusMessage_ = "Updated editor settings.";
		statusMessageIsError_ = false;
		return;
	}

	statusMessage_ = "Failed to update Config/EditorConfig.ini.";
	statusMessageIsError_ = true;
}
