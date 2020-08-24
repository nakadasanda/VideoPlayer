#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QThread>
#include <QImage>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/time.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"

    #include <SDL2/SDL.h>
    #include <SDL2/SDL_audio.h>
    #include <SDL2/SDL_types.h>
    #include <SDL2/SDL_name.h>
    #include <SDL2/SDL_main.h>
    #include <SDL2/SDL_config.h>
}


typedef struct PacketQueu{
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
}PacketQueu;

#define VIDEO_PICTURE_QUEUE_SIZE 1
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1sec 48khz 32bit audio 48000*4

typedef struct VideoState{
    AVCodecContext *aCodecCtx;
    AVFrame *audioFrame;
    PacketQueu *audioq;

    double Video_clock; ///pts od last decoded frame /predcated pts of next of decoded frame.

    AVStream *Video_st;
}VideoState;

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

    VideoState mVideoState;

};

#endif // VIDEOPLAYER_H
