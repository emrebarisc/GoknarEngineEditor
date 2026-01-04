#pragma once

#include "UI/EditorContext.h"
#include "EditorPanel.h"
#include <string>

class ObjectCreationPanel : public IEditorPanel
{
public:
    ObjectCreationPanel(EditorHUD* hud) : 
        IEditorPanel("Object Creation", hud)
    {

    }
    
    virtual void Draw() override;

private:
    void SetObjectToCreate(EditorSelectionType type, const std::string& name = "");
};