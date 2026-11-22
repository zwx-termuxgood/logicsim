#include <QGuiApplication>
// 引入 GUI 应用类：QGuiApplication 是 QML 应用的基础，管理事件循环与窗口系统
#include <QQmlApplicationEngine>
// 引入 QML 应用引擎：负责加载 QML 文件/模块并创建对象树
#include <QQuickStyle>
// 引入快速控件样式：用于设置 Qt Quick Controls 的样式（此处设为 Basic）

#ifdef Q_OS_ANDROID
// Android 平台条件编译：仅 Android 下才引入 JNI 相关头文件，
// 桌面构建不产生任何额外依赖与二进制体积
#include <QCoreApplication>
// 引入 QCoreApplication：QNativeInterface::QAndroidApplication::context() 需要它
#include <QJniObject>
// 引入 QJniObject：从 C++ 调用 Android Java API（通过 JNI）的核心封装
#endif

#include "model/Circuit.h"
// 引入 Circuit 类完整定义：qmlRegisterType 注册它到 QML 需要完整类型

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    // ★ 方案一：禁用 Qt Android 无障碍支持
    //   原因：Qt 6.11.1 的无障碍模块在旧版 Android（如 API 29）上会调用
    //   AccessibilityNodeInfo$RangeInfo 的构造方法 (IFFF)V，该签名只在
    //   较新 API 中存在，导致 NoSuchMethodError。未清除的 JNI 异常在某些
    //   设备上可能导致崩溃（SIGABRT）。LogicSim 是画布编辑工具，不依赖
    //   TalkBack 等辅助功能，禁用无障碍对绝大多数用户无影响。
    //   必须在 QGuiApplication 创建之前设置，Qt 初始化时读取。
    qputenv("QT_ANDROID_DISABLE_ACCESSIBILITY", "1");

    // Android 平台条件编译：仅 Android 下执行以下设置
    qputenv("QSG_RHI_BACKEND", "opengl");
    // 设置 RHI 后端为 OpenGL：Android 上 OpenGL 兼容性更好，避免其它后端驱动问题
#endif
    // Android 条件编译结束

    QGuiApplication app(argc, argv);
    // 创建 GUI 应用对象，解析命令行参数

    app.setOrganizationName("LogicSim");
    // 设置组织名：QSettings 等按组织名区分存储位置
    app.setApplicationName("LogicSim");
    // 设置应用名：QSettings 等按应用名区分存储位置

    QQuickStyle::setStyle("Basic");
    // 设置 Qt Quick Controls 样式为 Basic：外观完全由 QML 自绘主题控制

    qmlRegisterType<Circuit>("LogicSim", 1, 0, "Circuit");
    // 把 C++ 的 Circuit 类型注册到 QML 模块 LogicSim 1.0，名为 Circuit

    QQmlApplicationEngine engine;
    // 创建 QML 应用引擎
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    // 连接对象创建失败信号：创建失败时退出应用（返回码 -1），
    // 用排队连接保证在事件循环中安全退出
    engine.loadFromModule("LogicSim", "Main");
    // 从 LogicSim 模块加载名为 Main 的 QML 组件作为主窗口

    return app.exec();
    // 进入事件循环，返回应用退出码
}