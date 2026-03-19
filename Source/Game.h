#include "Goknar/Application.h"

class EditorHUD;

class FreeCameraObject;

class GOKNAR_API Game : public Application
{
public:
	Game();

	~Game();

	void Run() override;

	void LoadProject(const std::string& projectPath);

private:
	EditorHUD* editorHUD_{ nullptr };
};