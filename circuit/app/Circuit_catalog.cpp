#include "../Circuit.h"
#include "../logic/ComponentLogic.h"

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