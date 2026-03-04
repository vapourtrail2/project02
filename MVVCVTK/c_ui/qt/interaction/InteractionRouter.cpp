#include "c_ui/qt/interaction/InteractionRouter.h"

void InteractionRouter::Add(std::unique_ptr<IInteractionHandler> handler)
{
    if (!handler) {
        return;
    }
    m_handlers.push_back(std::move(handler));
}

void InteractionRouter::Clear()
{
    m_handlers.clear();
}

InteractionResult InteractionRouter::Dispatch(const InteractionEvent& eve)
{
    for (const auto& handler : m_handlers) {
        if (!handler) {
            continue;
        }
        const InteractionResult result = handler->Handle(eve);
        if (result.handled) {
            return result;
        }
    }
    return {};
}
