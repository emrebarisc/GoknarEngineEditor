#pragma once

#include <string>
#include <vector>

namespace EditorSourceCodeUtils
{
	enum class SourceCodeEditor
	{
		Auto = 0,
		VisualStudio,
		VisualStudioCode,
		SystemDefault
	};

	struct SourceCodeEditorOption
	{
		SourceCodeEditor editor{ SourceCodeEditor::Auto };
		const char* label{ "" };
		const char* configValue{ "" };
		bool isAvailable{ false };
	};

	std::vector<SourceCodeEditorOption> GetSourceCodeEditorOptions();
	SourceCodeEditor GetPreferredSourceCodeEditor();
	bool SetPreferredSourceCodeEditor(SourceCodeEditor editor);

	bool IsSourceCodeEditorAvailable(SourceCodeEditor editor);
	const char* GetSourceCodeEditorLabel(SourceCodeEditor editor);
	const char* GetSourceCodeEditorConfigValue(SourceCodeEditor editor);
	SourceCodeEditor SourceCodeEditorFromConfigValue(const std::string& value);

	bool OpenSourceFile(const std::string& filePath);
	void ShowInFileExplorer(const std::string& path, bool selectPath);
}
