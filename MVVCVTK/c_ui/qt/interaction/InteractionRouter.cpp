//#include "InteractionRouter.h";
//
//void InteractionRouter::Add(std::unique_ptr<IInteractionHandler> handler) {
//	m_handlers.push_back(std::move(handler));
//}
//
//InteractionResult InteractionRouter::Dispatch(const InteractionEvent& eve) {
//	for (auto& h : m_handlers) {
//		InteractionResult r = h->Handle(eve);
//		if (r.handled) {
//			return r;
//		}
//	}
//	return {};//Œ¥¥¶¿Ì
//}
