#pragma once
#include "AppInterfaces.h"
#include <string>

class vtkRenderWindowInteractor;

struct InteractionEvent
{
    unsigned long vtkEventId = 0;
    vtkRenderWindowInteractor* iren = nullptr;

    int x = 0;
    int y = 0;

    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    char keyCode = 0;
    std::string keySym;

    VizMode vizMode = VizMode::Volume;
    ToolMode toolMode = ToolMode::Navigation;
};
