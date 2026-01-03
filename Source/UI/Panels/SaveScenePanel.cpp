#include "SaveScenePanel.h"

#include "imgui.h"

#include "Goknar/Application.h"
#include "Goknar/Engine.h"
#include "Goknar/Helpers/SceneParser.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorWidgets.h"

void SaveScenePanel::Draw()
{
	std::string resultText;
	if (EditorWidgets::DrawOneTextBoxOneButton(
		title_,
		"Path: ",
		EditorContext::Get()->sceneSavePath,
		"Save",
		Vector2i{ 400, 400 },
		resultText,
		isOpen_,
		ImGuiWindowFlags_NoResize))
	{
		SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + resultText);

		hud_->HidePanel<SaveScenePanel>();
	}
}