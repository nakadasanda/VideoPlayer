#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QDebug"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    mPlayer = new VideoPlayer();
    connect(mPlayer,SIGNAL(sig_GetOneFrame(QImage)),this,SLOT(slotGetOneFrame(QImage)));

    mPlayer->setFileName("/home/nakadasanda/Videos/Pokemon.mp4");
    mPlayer-> startPlay();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setBrush(Qt::black);
    painter.drawRect(0,0,this->width(),this->height());

    if(mImage.size().width() <= 0) return;

    QImage img = mImage.scaled(this->size(),Qt::KeepAspectRatio);

    painter.drawImage(0,0,img);

}

void MainWindow::slotGetOneFrame(QImage img)
{
    mImage = img;
    update();
}

