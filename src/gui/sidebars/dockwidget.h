// For license of this file, see <project-root-folder>/LICENSE.md.

#include <QDockWidget>

#ifndef DOCKWIDGET_H
#define DOCKWIDGET_H

class TextApplication;

class DockWidget : public QDockWidget {
  Q_OBJECT

  public:
    explicit DockWidget(TextApplication* text_app, QWidget* parent = nullptr);
    virtual ~DockWidget() = default;

    void setTextApplication(TextApplication* text_app);

    virtual Qt::DockWidgetArea initialArea() const = 0;
    virtual bool initiallyVisible() const = 0;
    virtual int initialWidth() const = 0;

  public slots:
    void switchVisibility();
    virtual void load() = 0;

  protected:
    virtual void showEvent(QShowEvent* event) override;

  protected:
    TextApplication* m_textApp;
};

#endif // DOCKWIDGET_H
