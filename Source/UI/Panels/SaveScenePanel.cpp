#include "SaveScenePanel.h"

#include "imgui.h"

#include "Goknar/Application.h"
#include "Goknar/Engine.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorSceneSerializer.h"
#include "UI/EditorWidgets.h"

void SaveScenePanel::Draw()
{
	std::string resultText;
	if (EditorWidgets::DrawWindowWithOneTextBoxOneButton(
		title_,
		"Path: ",
		EditorContext::Get()->sceneSavePath,
		"Save",
		Vector2i{ 400, 400 },
		resultText,
		isOpen_,
		ImGuiWindowFlags_NoResize))
	{
		EditorContext::Get()->sceneSavePath = resultText;
		hud_->PrepareSceneForSave();
		EditorSceneSerializer::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + resultText);
		EditorContext::Get()->ClearSceneDirty();

		hud_->HidePanel<SaveScenePanel>();
	}
}
