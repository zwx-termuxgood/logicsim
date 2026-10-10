#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QDateTime>

#include "CircuitContext.h"
#include "UndoData.h"

class EventEngine;

class Circuit : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Circuit)

    Q_PROPERTY(QVariantList components READ components NOTIFY changed)
    Q_PROPERTY(QVariantList wires READ wires NOTIFY changed)
    Q_PROPERTY(QString contextId READ contextId NOTIFY contextChanged)
    Q_PROPERTY(QString contextName READ contextName NOTIFY contextChanged)
    Q_PROPERTY(bool isRootContext READ isRootContext NOTIFY contextChanged)
    Q_PROPERTY(bool isViewOnly READ isViewOnly NOTIFY contextChanged)
    Q_PROPERTY(QVariantList subcircuits READ subcircuits NOTIFY subcircuitsChanged)
    Q_PROPERTY(QVariantList editContexts READ editContexts NOTIFY editContextsChanged)
    Q_PROPERTY(QVariantList errors READ errors NOTIFY errorsChanged)
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(bool clockRunning READ clockRunning NOTIFY clockChanged)
    Q_PROPERTY(int clockFrequency READ clockFrequency NOTIFY clockChanged)
    Q_PROPERTY(int clockTickCount READ clockTickCount NOTIFY clockChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)

    Q_PROPERTY(QString engineType READ engineType WRITE setEngineType NOTIFY engineChanged)
    Q_PROPERTY(bool simulationRunning READ simulationRunning NOTIFY simulationChanged)
    Q_PROPERTY(qulonglong simTime READ simTime NOTIFY simulationTick)
    Q_PROPERTY(qulonglong globalGateDelay READ globalGateDelay WRITE setGlobalGateDelay NOTIFY engineSettingsChanged)
    Q_PROPERTY(qulonglong wireDelay READ wireDelay WRITE setWireDelay NOTIFY engineSettingsChanged)
    Q_PROPERTY(qulonglong simulationSpeed READ simulationSpeed WRITE setSimulationSpeed NOTIFY engineSettingsChanged)
    Q_PROPERTY(int simulationSpeedExp READ simulationSpeedExp WRITE setSimulationSpeedExp NOTIFY engineSettingsChanged)
    Q_PROPERTY(bool autoSpeed READ autoSpeed WRITE setAutoSpeed NOTIFY engineSettingsChanged)

public:
    explicit Circuit(QObject* parent = nullptr);
    ~Circuit() override;

    QVariantList components() const;
    QVariantList wires() const;
    QString contextId() const { return m_currentCtxId; }
    QString contextName() const;
    bool isRootContext() const { return m_currentCtxId.isEmpty(); }
    bool isViewOnly() const { return m_viewOnly; }
    QVariantList subcircuits() const;
    QVariantList editContexts() const;
    QVariantList errors() const { return m_errors; }
    QVariantList recentFiles() const { return m_recentFiles; }
    bool clockRunning() const { return m_clockRunning; }
    int clockFrequency() const { return m_clockFrequency; }
    int clockTickCount() const { return m_clockTickCount; }

    bool canUndo() const;
    bool canRedo() const;

    QString engineType() const { return m_engineType; }
    void setEngineType(const QString& t);
    bool simulationRunning() const { return m_simulationRunning; }
    qulonglong simTime() const;
    qulonglong globalGateDelay() const;
    void setGlobalGateDelay(qulonglong d);
    qulonglong wireDelay() const;
    void setWireDelay(qulonglong d);
    qulonglong simulationSpeed() const { return m_simulationSpeed; }
    void setSimulationSpeed(qulonglong s);
    int simulationSpeedExp() const { return m_simulationSpeedExp; }
    void setSimulationSpeedExp(int e);
    bool autoSpeed() const { return m_autoSpeed; }
    void setAutoSpeed(bool a);

    Q_INVOKABLE void setGateTypeDelay(const QString& type, qulonglong d);
    Q_INVOKABLE qulonglong getGateTypeDelay(const QString& type) const;
    Q_INVOKABLE void clearGateTypeDelays();

    Q_INVOKABLE void startSimulation();
    Q_INVOKABLE void pauseSimulation();
    Q_INVOKABLE void resetSimulation();
    Q_INVOKABLE void simulateStep();

    static int componentInputCount(const QVariantMap& comp);
    static int componentOutputCount(const QVariantMap& comp);
    static int outputPortBitWidth(const QVariantMap& comp, int portIdx);
    static int inputPortBitWidth(const QVariantMap& comp, int portIdx);

    Q_INVOKABLE QString addComponent(const QString& type, double x, double y,
                                     int bitWidth = 1, int inputCount = 2,
                                     const QString& subId = QString());
    Q_INVOKABLE void removeComponent(const QString& id);
    Q_INVOKABLE void removeComponents(const QStringList& ids);
    Q_INVOKABLE void moveComponent(const QString& id, double x, double y);
    Q_INVOKABLE void moveSelection(const QStringList& ids, double dx, double dy);
    Q_INVOKABLE void toggleInput(const QString& id);
    Q_INVOKABLE void setBitValue(const QString& id, int bit, bool value);
    Q_INVOKABLE void setComponentProp(const QString& id, const QString& key, const QVariant& value);
    Q_INVOKABLE void renameComponent(const QString& id, const QString& name);
    Q_INVOKABLE void setSplitterSplits(const QString& id, const QString& splitsStr);
    Q_INVOKABLE QString getSplitterSplitsStr(const QString& id) const;
    Q_INVOKABLE void rotateComponent(const QString& id, int delta);
    Q_INVOKABLE void toggleClock(const QString& id);

    Q_INVOKABLE QString getInputBase(const QString& id) const;
    Q_INVOKABLE void setInputBase(const QString& id, const QString& base);
    Q_INVOKABLE QString getInputAsString(const QString& id) const;
    Q_INVOKABLE QString setInputFromString(const QString& id, const QString& text);
    Q_INVOKABLE QString formatValue(const QString& binStr, const QString& base, int bitWidth) const;

    Q_INVOKABLE QString addWire(const QString& fromComp, int fromPort,
                                const QString& toComp, int toPort);
    Q_INVOKABLE void removeWire(const QString& id);

    Q_INVOKABLE QVariantMap getComponent(const QString& id) const;
    Q_INVOKABLE QVariantMap getWire(const QString& id) const;
    Q_INVOKABLE QVariantList outputPortsInfo(const QString& id) const;
    Q_INVOKABLE QVariantList inputPortsInfo(const QString& id) const;
    Q_INVOKABLE QVariantMap allPortsInfo() const;

    Q_INVOKABLE QString addSubcircuit(const QString& name);
    Q_INVOKABLE void deleteSubcircuit(const QString& subId);
    Q_INVOKABLE void setSubcircuitName(const QString& subId, const QString& name);
    Q_INVOKABLE QString getSubcircuitName(const QString& subId) const;
    Q_INVOKABLE bool canAddSubInstance(const QString& targetSubId) const;
    Q_INVOKABLE void setSubcircuitAsRoot(const QString& subId);

    Q_INVOKABLE void enterSubcircuit(const QString& subId);
    Q_INVOKABLE void leaveSubcircuit();
    Q_INVOKABLE void switchEditContext(const QString& ctxId);

    Q_INVOKABLE void refreshErrors();

    Q_INVOKABLE QVariantList copySelection(const QStringList& ids);
    Q_INVOKABLE QStringList pasteSelection(const QVariantList& data, double dx, double dy);

    Q_INVOKABLE void addRecentFile(const QString& path);
    Q_INVOKABLE void removeRecentFile(const QString& path);
    Q_INVOKABLE void clearRecentFiles();

    Q_INVOKABLE void clear();
    Q_INVOKABLE void evaluate();

    Q_INVOKABLE QVariantMap toJson() const;
    Q_INVOKABLE bool fromJson(const QVariantMap& data);
    Q_INVOKABLE bool saveToFile(const QString& path);
    Q_INVOKABLE bool loadFromFile(const QString& path);

    Q_INVOKABLE QString wireAtPoint(double x, double y, double tol) const;

    Q_INVOKABLE void startClock();
    Q_INVOKABLE void stopClock();
    Q_INVOKABLE void setClockFrequency(int hz);
    Q_INVOKABLE void singleStep();
    Q_INVOKABLE void resetAllInputs();

    Q_INVOKABLE QVariantMap statistics() const;

    Q_INVOKABLE QString addText(double x, double y, const QString& text = QString("文本"));
    Q_INVOKABLE void setTextContent(const QString& id, const QString& content);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void breakUndoMerge();

    void dumpInternalState(const QString& where) const;

signals:
    void changed();
    void geometryChanged();
    void contextChanged();
    void subcircuitsChanged();
    void editContextsChanged();
    void errorsChanged();
    void recentFilesChanged();
    void clockChanged();
    void undoRedoChanged();

    void engineChanged();
    void simulationChanged();
    void simulationTick();
    void engineSettingsChanged();

    // 结构变化：元件集合 / 元件属性（位宽、inputCount、rotation、splits）
    // / 上下文切换 / undo-redo / 加载。UI 收到后需要重建端口信息缓存。
    void structureChanged();

private slots:
    void tick();

private:
    using Context = CircuitContext;

    Context m_root;
    QHash<QString, Context> m_subContexts;
    QHash<QString, QString> m_subNames;
    QString m_currentCtxId;
    bool m_viewOnly = false;
    int m_subCounter = 0;
    QVariantList m_errors;
    QVariantList m_recentFiles;

    bool m_clockRunning = false;
    int m_clockFrequency = 2;
    int m_clockTickCount = 0;
    QTimer* m_clockTimer = nullptr;
    QHash<QString, bool> m_clockStates;

    QString m_engineType = "event";
    bool m_simulationRunning = false;
    bool m_engineDirty = true;
    EventEngine* m_eventEngine = nullptr;
    QTimer* m_simTimer = nullptr;
    qulonglong m_simulationSpeed = 1000;
    int m_simulationSpeedExp = 3;
    bool m_autoSpeed = true;

    CircuitUndoData* und() const;

    Context& currentCtx();
    const Context& currentCtx() const;
    Context* ctxById(const QString& id);
    const Context* ctxById(const QString& id) const;

    int indexOfComponent(const QString& id) const;
    int indexOfWire(const QString& id) const;

    void evaluateAll();

    void loadRecentFiles();
    void saveRecentFiles();

    bool hasSubCycle(const QString& fromCtx, const QString& targetId, QSet<QString>& visited) const;

    void pushUndo(const QString& mergeKey = QString());
    void restoreSnapshot(const QVariantMap& snap, bool emitSignals = true);

    void rebuildEventEngine();
    void applyEngineOutputsToContexts();
    void onSimTimerTick();

    QStringList engineQidsFor(const QString& ctxId, const QString& localCompId) const;

    void syncSubStateFromParent(const QString& subId);
    void applyEngineOutputsToSubContext();
};

#endif // CIRCUIT_H