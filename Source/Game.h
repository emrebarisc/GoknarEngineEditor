#include "Goknar/Application.h"

class EditorHUD;

class FreeCameraObject;
class MaterialInitializer;

class GOKNAR_API Game : public Application
{
public:
	Game();

	~Game();

	void Run() override;

	FreeCameraObject* GetFreeCameraObject()
	{
		return freeCameraObject_;
	}

private:
	EditorHUD* editorHUD_{ nullptr };
	FreeCameraObject* freeCameraObject_{ nullptr };
	MaterialInitializer* materialInitializer_{ nullptr };

};