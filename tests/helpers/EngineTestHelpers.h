#pragma once
#include "engine/event/EventEngine.h"
// 分批推进事件引擎；step 是每批推进的时间范围，budget 是每批处理事件的预算。
// 最多尝试 1000 批，防止反馈电路让测试无限循环；退出本函数不等于保证已经空闲，
// 需要收敛的调用方仍应检查 isIdle() 或相应输出，避免把预算耗尽误认为求解完成。
inline void runToIdle(EventEngine& eng, quint64 step = 100000, quint64 budget = 2000000) {
    for (int i = 0; i < 1000 && !eng.isIdle(); ++i)
        eng.run(step, budget);
}
