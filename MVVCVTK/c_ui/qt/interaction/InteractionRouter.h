#pragma once
#include <memory>
#include <vector>
#include "IInteractionHandler.h"

class InteractionRouter
{
public:
    void Add(std::unique_ptr<IInteractionHandler> handler);
    InteractionResult Dispatch(const InteractionEvent& eve);

private:
    std::vector<std::unique_ptr<IInteractionHandler>> m_handlers;
};
