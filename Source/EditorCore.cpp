#include "EditorCore.h"

#include "Goknar/mainThread.h"

#ifdef GOKNAR_PLATFORM_WINDOWS
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

std::string EditorContentDir = "EditorContent/";
