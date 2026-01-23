#pragma once
#include "AppInterfaces.h"

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

	VizMode vizMode = VizMode::Volume;
	ToolMode toolMode = ToolMode::Navigation;
};