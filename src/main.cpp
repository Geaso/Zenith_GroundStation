#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include "AppState.h"
#include "ScriptActionModel.h"
#include "ZenithProtocolProfile.h"
#include "GridMapImageProvider.h"
#include "VoxelInstanceTable.h"
#include "TrailInstanceTable.h"
#include "PlannedPathInstanceTable.h"

static QFile logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " ";
    switch (type) {
    case QtDebugMsg:    out << "DBG "; break;
    case QtInfoMsg:     out << "INF "; break;
    case QtWarningMsg:  out << "WRN "; break;
    case QtCriticalMsg: out << "CRT "; break;
    case QtFatalMsg:    out << "FTL "; break;
    }
    out << msg << "\n";
    out.flush();
}

int main(int argc, char *argv[])
{
    // 调试日志默认关闭，避免向客户暴露内部运行细节；需要排障时加 --debug 启动。
    bool debugLogging = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--debug") == 0) {
            debugLogging = true;
            break;
        }
    }
    if (debugLogging) {
        logFile.setFileName("zenith_debug.log");
        logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        qInstallMessageHandler(messageHandler);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Zenith Ground Station");
    QGuiApplication::setOrganizationName("AMOVLAB");
    QGuiApplication::setWindowIcon(QIcon(":/Zenith.ico"));
    QQuickStyle::setStyle("Basic");

    AppState appState;
    ScriptActionModel scriptActionModel;
    ZenithProtocolProfile zenithProtocolProfile;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appState", &appState);
    engine.rootContext()->setContextProperty("scriptActionModel", &scriptActionModel);
    engine.rootContext()->setContextProperty("protocolProfile", &zenithProtocolProfile);

    engine.addImageProvider("gridmap", new GridMapImageProvider(appState.telemetryStore()));
    qmlRegisterType<VoxelInstanceTable>("Zenith3D", 1, 0, "VoxelInstanceTable");
    qmlRegisterType<TrailInstanceTable>("Zenith3D", 1, 0, "TrailInstanceTable");
    qmlRegisterType<PlannedPathInstanceTable>("Zenith3D", 1, 0, "PlannedPathInstanceTable");
    engine.rootContext()->setContextProperty("telemetryStore", appState.telemetryStore());

    // QML 全部编进 ZenithUI 模块的字节码，不需要任何外部 import 路径。
    const QUrl url(QStringLiteral("qrc:/qt/qml/ZenithUI/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
