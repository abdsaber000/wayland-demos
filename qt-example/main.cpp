#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QtWaylandClient/QtWaylandClient>
#include <QtGui/6.9.0/QtGui/qpa/qplatformnativeinterface.h>
#include "vlc_player.h"
#include <iostream>

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(const QString &mediaPath, QWidget *parent = nullptr)
        : QWidget(parent), m_mediaPath(mediaPath)
    {
        // Ensure we have a native window
        setAttribute(Qt::WA_NativeWindow);
    }

    ~VideoWidget() {
        if (m_player) {
            delete m_player;
        }
    }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (!m_initialized) {
            initializeVLC();
        }
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (m_player) {
            m_player->set_size(width(), height());
        }
    }

private:
    void initializeVLC() {
        // Get the QWindow handle from this widget
        QWindow *window = this->windowHandle();
        if (!window) {
            qFatal("Failed to get window handle");
        }

        QPlatformNativeInterface *native = 
            QGuiApplication::platformNativeInterface();
        
        wl_display *display = static_cast<wl_display*>(
            native->nativeResourceForIntegration("wl_display"));
        
        wl_surface *surface = static_cast<wl_surface*>(
            native->nativeResourceForWindow("surface", window));

        std::cout << "Display: " << display << " Surface: " << surface << std::endl;
        
        if (!display || !surface) {
            qFatal("Failed to get Wayland resources");
        }

        m_player = new VlcPlayer(0, nullptr);
        m_player->open_media(m_mediaPath.toUtf8().constData());
        
        m_player->set_render_window({
            .display = display,
            .surface = surface,
            .width = width(),
            .height = height()
        });
        m_player->play();
        m_player->set_size(width(), height());
        
        m_initialized = true;
    }

    VlcPlayer *m_player = nullptr;
    QString m_mediaPath;
    bool m_initialized = false;
};

int main(int argc, char *argv[])
{
    // Set Wayland environment
    qputenv("QT_QPA_PLATFORM", "wayland");
    // qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    
    QApplication app(argc, argv);

    if (app.arguments().size() < 2) {
        qCritical("Usage: %s <media-file>", argv[0]);
        return 1;
    }

    // Create main widget
    QWidget mainWidget;
    mainWidget.setWindowTitle("VLC Wayland Player");
    mainWidget.resize(800, 600);
    
    // Create layout
    QVBoxLayout *layout = new QVBoxLayout(&mainWidget);
    
    // Add video widget
    VideoWidget *videoWidget = new VideoWidget(app.arguments().at(1));
    layout->addWidget(videoWidget);
    
    mainWidget.show();

    return app.exec();
}

#include "main.moc"