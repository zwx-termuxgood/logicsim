#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "circuit.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    qputenv("QSG_RHI_BACKEND", "opengl");
#endif

    QGuiApplication app(argc, argv);
    app.setOrganizationName("LogicSim");
    app.setApplicationName("LogicSim");

    QQuickStyle::setStyle("Basic");

    qmlRegisterType<Circuit>("LogicSim", 1, 0, "Circuit");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("LogicSim", "Main");

    return app.exec();
}