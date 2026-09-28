#include "sync/delivery.hpp"

namespace lit::game {
DropVerdict note_drop(ClientSync& sync, std::uint32_t tick, std::uint32_t window_ticks) {
    const bool dropped_before = sync.drop_tick != 0 && sync.drop_tick != tick;
    if (dropped_before && tick - sync.drop_tick < window_ticks) {
        return DropVerdict::Close;
    }
    sync.drop_tick = tick;
    sync.resync = true;
    return DropVerdict::Resync;
}
}  // namespace lit::game
