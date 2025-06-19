#include <QGuiApplication>
#include <QWindow>
#include <QtWaylandClient/QtWaylandClient>
#include <QtGui/6.9.0/QtGui/qpa/qplatformnativeinterface.h>
#include "vlc_player.h"
#include <cstdio>


class VLCWaylandWindow : public QWindow
{
    Q_OBJECT

public:
    explicit VLCWaylandWindow(const QString &mediaPath, QWindow *parent = nullptr)
        : QWindow(parent), m_mediaPath(mediaPath)
    {
        setSurfaceType(QSurface::OpenGLSurface);
        create();
    }

    ~VLCWaylandWindow() {
        if (m_player) {
            delete m_player;
        }
    }

protected:
    void exposeEvent(QExposeEvent *) override {
        if (isExposed() && !m_initialized) {
            initializeVLC();
        }
    }

    void resizeEvent(QResizeEvent *event) override {
        if (m_player) {
            m_player->set_size(event->size().width(), event->size().height());
        }
        QWindow::resizeEvent(event);
    }

private:
    void initializeVLC() {
        QPlatformNativeInterface *native = 
            QGuiApplication::platformNativeInterface();
        
        wl_display *display = static_cast<wl_display*>(
            native->nativeResourceForIntegration("wl_display"));
        
        wl_surface *surface = static_cast<wl_surface*>(
            native->nativeResourceForWindow("surface", this));
        std::cout << display << std::endl;
        if (!display || !surface) {
            qFatal("Failed to get Wayland resources");
        }


        m_player = new VlcPlayer(0, NULL);
        m_player->open_media(m_mediaPath.toUtf8().constData());
        
        m_player->set_render_window({
            .display = display,
            .surface = surface,
            .width = width(),
            .height = height()
        });

        m_player->play();
        
        m_initialized = true;
    }

    VlcPlayer *m_player = nullptr;
    QString m_mediaPath;
    bool m_initialized = false;
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    
    QGuiApplication app(argc, argv);

    if (app.arguments().size() < 2) {
        qCritical("Usage: %s <media-file>", argv[0]);
        return 1;
    }

    VLCWaylandWindow window(app.arguments().at(1));
    window.setGeometry(100, 100, 1280, 720);
    window.show();

    return app.exec();
}

#include "main.moc"