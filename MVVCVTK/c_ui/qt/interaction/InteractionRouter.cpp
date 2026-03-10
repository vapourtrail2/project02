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

InteractionResult InteractionRouter::Dispatch(const InteractionEvent& eve, RouterDispatchMode mode)
{
    InteractionResult aggregated;

    for (const auto& handler : m_handlers) {
        if (!handler) {
            continue;
        }

        const InteractionResult result = handler->Handle(eve);
        if (result.abortVtk) {
            aggregated.abortVtk = true;
        }

        if (mode == RouterDispatchMode::FirstMatch && result.handled) {
            aggregated.handled = true;
            return aggregated;
        }

        if (result.handled) {
            aggregated.handled = true;
        }
    }

    return aggregated;
}
