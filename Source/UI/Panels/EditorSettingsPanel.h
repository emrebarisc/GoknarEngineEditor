#pragma once

#include "EditorPanel.h"

class EditorSettingsPanel : public IEditorPanel
{
public:
	EditorSettingsPanel(EditorHUD* hud);

	void Init() override;
	void Draw() override;

private:
	void LoadSettings();
	bool ApplyGammaCorrection();
	void SaveGammaCorrection();

	float gammaCorrection_{ 2.2f };
	std::string statusMessage_{};
	bool statusMessageIsError_{ false };
};
