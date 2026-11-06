#pragma once
#include "engine/event/EventEngine.h"
inline void runToIdle(EventEngine& eng, quint64 step = 100000, quint64 budget = 2000000) {
    for (int i = 0; i < 1000 && !eng.isIdle(); ++i)
        eng.run(step, budget);
}
