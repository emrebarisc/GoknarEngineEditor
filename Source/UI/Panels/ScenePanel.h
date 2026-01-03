#pragma once

#include "EditorPanel.h"
#include <vector>

class ObjectBase;

class ScenePanel : public IEditorPanel
{
public:
    ScenePanel(EditorHUD* hud) : 
        IEditorPanel("Scene", hud) 
    {

    }
    
    virtual void Draw() override;

private:
    void DrawSceneLights();
    void DrawSceneObjects();
    void DrawSceneObject(ObjectBase* object);
};