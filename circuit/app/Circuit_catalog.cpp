#include "../Circuit.h"
#include "../logic/ComponentLogic.h"

// ============================================================
// 元件目录：Circuit 静态函数转发到 ComponentLogic
//
// 这些 static 函数是 QML 侧通过 Circuit::componentInputCount(...)
// 等方式调用元件基础属性的唯一入口。集中在此，便于 QML 侧查询，
// 也便于将来若需要从注册表导出元件清单时在此扩展。
// ============================================================

int Circuit::componentInputCount(const QVariantMap& comp) {
    return ComponentLogic::inputCount(comp);
}

int Circuit::componentOutputCount(const QVariantMap& comp) {
    return ComponentLogic::outputCount(comp);
}

int Circuit::outputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentLogic::outputBitWidth(comp, portIdx);
}

int Circuit::inputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentLogic::inputBitWidth(comp, portIdx);
}
