#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "AppState.h"
#include "ScriptActionModel.h"
#include "ZenithProtocolProfile.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Zenith Ground Station");
    QGuiApplication::setOrganizationName("AMOVLAB");
    QGuiApplication::setWindowIcon(QIcon(":/Zenith.png"));
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
