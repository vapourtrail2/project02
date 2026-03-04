#pragma once

struct InteractionResult
{
	bool handled = false;
	bool abortVtk = false;//是否要阻止 VTK 继续处理这个事件
};