extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
}

#include <iostream>

using namespace std;

/// SaveFrame関数
/// RGB情報を1つの画像ファイルに保存します。
void SaveFrame(AVFrame *pFrame, int width, int height,int index)
{
    FILE *pFile;
    char szFilename[32];
    int y;

    //Openfile
    sprintf(szFilename,"frame%d.ppm",index);
    pFile = fopen(szFilename,"wb");

    if(pFile==NULL) return;

    //Write header
    fprintf(pFile,"P6\n%d %d\n255\n", width, height);

    //Write pixel data
    for(y=0;y<height;y++)
    {
        fwrite(pFrame->data[0]+y*pFrame->linesize[0],1,width*3,pFile);
    }

    //close file
    fclose(pFile);

}

int main()
{
    char* file_path = "D:\\Amthem.mp4";

    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameRGB;
    AVPacket *packt;
    uint8_t *out_buffer;

    static struct SwsContext *img_convert_ctx;

    int videoStream, i, numBytes;
    int ret, got_picture;


    av_register_all();  //FFMPEGは、これを使用してアプリケーションを初期化します。
    cout << "Hello FFmpeg!" << endl;
    unsigned version = avcodec_version();
    cout << "version is:" << version << endl;

    //AVFormatContextを割り当てます.
    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx,file_path,NULL,NULL) != 0)
    {
        cout << "Dond read file" << endl;
        return -1;
    }

    cout << "Read file" << endl;

    if(avformat_find_stream_info(pFormatCtx,NULL) < 0)
    {
        cout << "Can't find stream infomation" << endl;
        return -1;
    }

    cout << "find stream infomation" << endl;

    videoStream = -1;

    //ビデオのストリームを見つかるまで探索します。
    //videoStreamに保存されます
    //ここでは、ビデオストリームだけを扱います.
    for(i=0;i < pFormatCtx->nb_streams;i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
    }

    //videoストリームが-1の場合ビデオストリームが見つかりませんでした。
    if(videoStream == -1)
    {
        cout << "Don't find stream " << endl;
        return -1;
    }

    cout << "find stream " << endl;

    //デコーダーを見つける
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    //デコーダーを開く
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
                                     AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB24,pCodecCtx->width, pCodecCtx->height);

    out_buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB,out_buffer,AV_PIX_FMT_RGB24,pCodecCtx->width, pCodecCtx->height);

    int y_size = pCodecCtx->width * pCodecCtx->height;

    packt = (AVPacket *) malloc(sizeof(AVPacket)); //パケット割り当て
    av_new_packet(packt,y_size);    //パケットデータ割り当て

    av_dump_format(pFormatCtx,0,file_path,0);   //ビデオ情報を出力する

    int index = 0;

    while (1) {
        if(av_read_frame(pFormatCtx,packt) < 0)
        {
            break; //ビデオが読み込まれます。
        }

        if(packt -> stream_index == videoStream)
        {
            ret = avcodec_decode_video2(pCodecCtx,pFrame,&got_picture,packt);
            if(ret < 0)
            {
                cout << "decode error " << endl;
                return -1;
            }

            if(got_picture){
                sws_scale(img_convert_ctx,
                          (uint8_t const *const *)pFrame->data,
                          pFrame->linesize,0,pCodecCtx->height,pFrameRGB->data,
                          pFrameRGB->linesize);
                SaveFrame(pFrameRGB,pCodecCtx->width,pCodecCtx->height,index++);
                if(index > 1000) return 0; //1000枚の画像を保存します。
            }
        }
        av_free_packet(packt);
    }
    av_free(out_buffer);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
