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
    logFile.setFileName("zenith_debug.log");
    logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    qInstallMessageHandler(messageHandler);
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

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
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
