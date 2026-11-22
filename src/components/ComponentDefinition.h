#pragma once

#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <QStringList>
#include <memory>

namespace Components {
enum class Role { Unknown, Combinational, Input, Constant, Clock, Output, Led, Text, Subcircuit };

// A builtin translation unit owns its type's complete definition. Factory names
// follow create<FileStem>Definition; CMake discovers and explicitly references them.
class Definition {
public:
    virtual ~Definition() = default;
    virtual QString typeId() const = 0;
    virtual Role role() const { return Role::Combinational; }
    virtual int inputCount(const QVariantMap&) const { return 0; }
    virtual int outputCount(const QVariantMap&) const { return 0; }
    virtual int inputWidth(const QVariantMap& c, int) const;
    virtual int outputWidth(const QVariantMap& c, int) const;
    virtual QVariantList inputPorts(const QVariantMap&) const { return {}; }
    virtual QVariantList outputPorts(const QVariantMap&) const { return {}; }
    virtual QVector<QString> compute(const QVector<QString>&, int, const QVariantList&) const { return {}; }
    virtual void defaults(QVariantMap& c, const QString& subId) const;
};

const Definition& definition(const QString& typeId);
QStringList typeIds();

// Shared mechanics, not a type dispatcher. Layouts return unrotated ports;
// ComponentLogic applies rotation and integer quantization once for every type.
int width(const QVariantMap& c);
int listWidth(const QVariantMap& c, const char* key, int port, int fallback);
QVariantList sidePorts(const Definition& d, const QVariantMap& c, bool output,
                       double w = 80, double minimumHeight = 60,
                       int heightCount = -1);
QVariantList singlePort(const Definition& d, const QVariantMap& c, bool output,
                        double x, double y = 0, int index = 0);
void zeroPorts(const Definition& d, QVariantMap& c, int inputs = -1, int outputs = -1);
QChar bitAt(const QString& s, int bit);
QString input(const QVector<QString>& inputs, int index, int width);

enum class Reduction { And, Or, Xor };
QVector<QString> reduce(const QVector<QString>& inputs, int width, Reduction op, bool invert);
enum class Switch { Transmission, Nmos, Pmos };
QVector<QString> switchOutputs(const QVector<QString>& inputs, int width, Switch mode);

class GateDefinition : public Definition {
public:
    int inputCount(const QVariantMap& c) const override;
    int outputCount(const QVariantMap&) const override { return 1; }
    QVariantList inputPorts(const QVariantMap& c) const override;
    QVariantList outputPorts(const QVariantMap& c) const override;
};
class SourceDefinition : public Definition {
public:
    int outputCount(const QVariantMap&) const override { return 1; }
    QVariantList outputPorts(const QVariantMap& c) const override;
};
class TransistorDefinition : public Definition {
public:
    int inputCount(const QVariantMap&) const override { return 2; }
    int outputCount(const QVariantMap&) const override { return 1; }
    void defaults(QVariantMap& c, const QString&) const override;
};
} // namespace Components
