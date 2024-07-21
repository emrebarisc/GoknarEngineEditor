#include "Goknar/Application.h"

class EditorHUD;

class GOKNAR_API Game : public Application
{
public:
	Game();

	~Game();

	void Run() override;

private:
	EditorHUD* editorHUD_;
};