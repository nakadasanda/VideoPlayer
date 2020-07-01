#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>

#include "videoplayer/videoplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event);
private:
    Ui::MainWindow *ui;

    VideoPlayer *mPlayer;

    QImage mImage;

private slots:
    void slotGetOneFrame(QImage img);
};
#endif // MAINWINDOW_H
