#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QThread>
#include <QImage>

class VideoPlayer : public QThread
{
    Q_OBJECT
public:
    explicit VideoPlayer();
    ~VideoPlayer();

    void setFileName(QString path){mFileName = path;}
    void startPlay();

signals:
    void sig_GetOneFrame(QImage img );
protected:
    void run();
private:
    QString mFileName;

};

#endif // VIDEOPLAYER_H
