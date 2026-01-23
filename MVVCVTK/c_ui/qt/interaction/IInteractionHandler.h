#pragma once
#include "InteractionEvent.h"
#include "InteractionResult.h"

class IInteractionHandler
{
public:
    virtual ~IInteractionHandler() = default;
    virtual InteractionResult Handle(const InteractionEvent& eve) = 0;
};
