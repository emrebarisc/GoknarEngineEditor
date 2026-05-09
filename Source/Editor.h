#include "Goknar/Application.h"

class EditorHUD;

class EditorFreeCameraObject;

class GOKNAR_API Editor : public Application
{
public:
	Editor();

	~Editor();

	void Run() override;

	void LoadProject(const std::string& projectPath);

private:
	EditorHUD* editorHUD_{ nullptr };
};