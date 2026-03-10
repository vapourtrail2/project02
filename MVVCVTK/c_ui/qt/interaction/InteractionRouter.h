#pragma once
#include <memory>
#include <vector>
#include "IInteractionHandler.h"

enum class RouterDispatchMode
{
    FirstMatch,
    Broadcast
};

class InteractionRouter
{
public:
    void Add(std::unique_ptr<IInteractionHandler> handler);
    void Clear();
    InteractionResult Dispatch(
        const InteractionEvent& eve,
        RouterDispatchMode mode = RouterDispatchMode::FirstMatch);

private:
    std::vector<std::unique_ptr<IInteractionHandler>> m_handlers;
};
