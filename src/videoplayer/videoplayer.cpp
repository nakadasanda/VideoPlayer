#include "videoplayer.h"

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
}

#include <iostream>

using namespace std;

VideoPlayer::VideoPlayer()
{
}
VideoPlayer::~VideoPlayer()
{

}

void VideoPlayer::startPlay()
{
    this->start();
}

void VideoPlayer::run()
{
    char file_path[512] = {0};
    strcpy(file_path,mFileName. toUtf8().data());

    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameRGB;
    AVPacket *packt;
    uint8_t *out_buffer;

    static struct SwsContext *img_convert_ctx;

    int videoStream, i, numBytes;
    int ret, got_picture;


    av_register_all();
    cout << "Hello FFmpeg!" << endl;
    unsigned version = avcodec_version();
    cout << "version is:" << version << endl;


    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx,file_path,NULL,NULL) != 0)
    {
        cout << "Dond read file" << endl;
        return ;
    }

    cout << "Read file" << endl;

    if(avformat_find_stream_info(pFormatCtx,NULL) < 0)
    {
        cout << "Can't find stream infomation" << endl;
        return ;
    }

    cout << "find stream infomation" << endl;

    videoStream = -1;

    for(i=0;i < pFormatCtx->nb_streams;i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
    }

    if(videoStream == -1)
    {
        cout << "Don't find stream " << endl;
        return;
    }

    cout << "find stream " << endl;

    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if(pCodec == NULL)
    {
        cout << "not find codec " << endl;
    }

    cout << "codec find " << endl;

    if(avcodec_open2(pCodecCtx,pCodec,NULL)<0)
    {
        cout << "not open codec " << endl;
    }
    cout << "codec open " << endl;

    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB32,pCodecCtx->width, pCodecCtx->height);

    out_buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB,out_buffer,AV_PIX_FMT_RGB32,pCodecCtx->width, pCodecCtx->height);

    int y_size = pCodecCtx->width * pCodecCtx->height;

    packt = (AVPacket *) malloc(sizeof(AVPacket));
    av_new_packet(packt,y_size);

    av_dump_format(pFormatCtx,0,file_path,0);

    int index = 0;

    while (1) {
        if(av_read_frame(pFormatCtx,packt) < 0)
        {
            break;
        }

        if(packt -> stream_index == videoStream)
        {
            ret = avcodec_decode_video2(pCodecCtx,pFrame,&got_picture,packt);
            if(ret < 0)
            {
                cout << "decode error " << endl;
                return;
            }

            if(got_picture){
                sws_scale(img_convert_ctx,
                          (uint8_t const *const *)pFrame->data,
                          pFrame->linesize,0,pCodecCtx->height,pFrameRGB->data,
                          pFrameRGB->linesize);

                QImage tempImg = QImage((uchar *)out_buffer,pCodecCtx->width,pCodecCtx->height,QImage::Format_RGB32);
                QImage image = tempImg.copy();
                emit sig_GetOneFrame(image);
            }
        }
        usleep(2000);
        av_free_packet(packt);
    }
    av_free(out_buffer);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return;
}
