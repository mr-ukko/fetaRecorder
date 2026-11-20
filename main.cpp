#undef new
#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <windows.h>
#include <QTimer>
#undef min
#undef max
#include "backend.h"
#include "screenrecord.h"

int main(int argc, char *argv[])
{
    #ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    #endif

    HRESULT hrCo = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        qCritical() << "Failed to initialize Media Foundation:" << Qt::hex << hr;
        return -1;
    }

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    ScreenRecord sScreenRecord;

    if (sScreenRecord.InitializeDXGI()) {
        sScreenRecord.StartRecording(60); 
    } else {
        qWarning() << "Warning: DXGI Screen Capture failed to initialize on startup.";
        // Continuing execution so QML GUI still opens for debugging!
    }

    engine.rootContext()->setContextProperty("cppVideo", &sScreenRecord);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    engine.loadFromModule("Fetakuvaaja", "Main");

    int ret = app.exec();

    MFShutdown();
    CoUninitialize();

    return ret;
}