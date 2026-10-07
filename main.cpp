#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "circuit.h"

int main(int argc, char *argv[])
{
    // Android 上强制使用 OpenGL ES 后端，避免部分驱动下 Vulkan 全屏切换渲染异常
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