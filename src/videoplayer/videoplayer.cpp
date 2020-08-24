#include "videoplayer.h"
#include <iostream>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

using namespace std;


void packet_queu_init (PacketQueu *q){
    memset(q,0,sizeof(PacketQueu));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int  packet_queu_put(PacketQueu *q, AVPacket *pkt){

    AVPacketList *pkt1;
    if(av_dup_packet((pkt)) < 0){
        exit(-1);
    }

    pkt1 = (AVPacketList * )av_malloc(sizeof(AVPacketList));

    if(!pkt1)
        exit(-1);

    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if(!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt =pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int packet_queu_get(PacketQueu *q,AVPacket *pkt, int block){
    AVPacketList *pkt1;
    int ret;
    SDL_LockMutex(q->mutex);

    for(;;){
        pkt1 = q->first_pkt;
        if(pkt1){
            q->first_pkt = pkt1->next;
            if(!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            break;
            ret =1;
        }
        else if(!block){
            ret = 0;
            break;
        }else {
            SDL_CondWait(q->cond,q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
}



int audio_decode_frame(VideoState *is,uint8_t *audiobuf,int bufsize){
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    int len1,datasize;

    AVCodecContext *aCodecctx = is->aCodecCtx;
    AVFrame *audioFrame = is->audioFrame;
    PacketQueu *audioq = is->audioq;

    for(;;)
    {
        if(packet_queu_get(audioq,&pkt,1)<0)
        {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
        while(audio_pkt_size >0)
        {
            int got_audio;

            int ret =avcodec_decode_audio4(aCodecctx,audioFrame,&got_audio,&pkt);
            if(ret < 0){
                printf("Error in decoding audio");
                exit(-1);
            }

            if(got_audio){
                int in_samples = audioFrame->nb_samples;
                short *sample_buffer = (short*)malloc(audioFrame->nb_samples *2 *2); //chanel *2 ,buffer *2
                memset(sample_buffer,0,audioFrame->nb_samples * 4);
                int i=0;
                float *inputChnnel0 = (float*)(audioFrame->extended_data[0]);

                if(audioFrame->channels ==1){
                    for(i=0;i<in_samples;i++){
                        float sample = *inputChnnel0++;
                        if(sample < -1.0f){
                            sample = -1.0f;
                        }else if(sample > 1.0f){
                            sample =1.0f;
                        }
                        sample_buffer[i] = (int16_t)(sample * 32767.0f);
                    }
                }else if(audioFrame->channels ==2 ){
                    float *inputChanel1 =(float*)(audioFrame->extended_data[1]);
                    for(i=0;i<in_samples;i++){
                        sample_buffer[i*2] = (int16_t)((*inputChnnel0++)*32767.0f);
                        sample_buffer[i*2+1] = (int16_t)((*inputChnnel0++)*32767.0f);
                    }
                }

                memcpy(audiobuf,sample_buffer,in_samples*4);
                free(sample_buffer);
            }

            audio_pkt_size -= ret;

            if(audioFrame->nb_samples <= 0)
            {
                continue;
            }

            datasize =audioFrame->nb_samples *4;

            return datasize;
        }
        if(pkt.data)
            av_free_packet(&pkt);
    }
}

void audio_callback(void *userdata,Uint8 *stream, int len)
{
    VideoState *is = (VideoState *)userdata;

    int len1,audio_data_size;

    static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3)/2];
    static unsigned int audio_buf_size =0;
    static unsigned int audio_buf_index =0;

    while(len >0){

        if(audio_buf_index >= audio_buf_size){
            audio_data_size = audio_decode_frame(is,audio_buf,sizeof(audio_buf));

            if(audio_data_size < 0){

                audio_buf_size = 1024;

                memset(audio_buf,0,audio_buf_size);
            }else{
                audio_buf_size = audio_data_size;
            }

            audio_buf_index = 0;
        }
        len1 = audio_data_size -audio_buf_index;

        if(len1 > len){
            len1 =len;
        }

        memcpy(stream,(uint8_t* )audio_buf + audio_buf_index,len1);
        len -=len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

static double synchronize_video(VideoState *is, AVFrame *src_frame, double pts){
    double frame_delay;

    if(pts != 0){
        is->Video_clock = pts;
    }else {
        pts = is->Video_clock;
    }

    frame_delay = av_q2d(is->Video_st->codec->time_base);
    frame_delay += src_frame->repeat_pict *(frame_delay *0.5);
    is->Video_clock += frame_delay;

    return pts;
}
VideoPlayer::VideoPlayer()
{
}
VideoPlayer::~VideoPlayer()
{

}

void VideoPlayer::startPlay()
{
    //QThreadのStart関数を呼び出すと、次のrun関すが実行されます。run関数は新しいスレッドです。
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

    AVCodecContext *audioCodecCtx;
    AVCodec *audioCodec;

    static struct SwsContext *img_convert_ctx;

    int audioStream, videoStream, i, numBytes;
    int ret, got_picture;


    av_register_all(); //FFMPEGは、これを使用してアプリケーションを初期化します。
    cout << "Hello FFmpeg!" << endl;
    unsigned version = avcodec_version();
    cout << "version is:" << version << endl;

    if(SDL_Init(SDL_INIT_AUDIO)){
        cout << "SDL Init Error" << endl;
        exit(1);
    }
    //AVFormatContextを割り当てます.
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
     //ビデオのストリームを見つかるまで探索します。
    //videoStreamに保存されます
    //ここでは、ビデオストリームだけを扱います.
    for(i=0;i < pFormatCtx->nb_streams;i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStream = i;
        }
    }

    if(videoStream == -1)
    {
        cout << "Don't find video stream " << endl;
        return;
    }

    if(audioStream == -1)
    {
        cout << "Don't find audio stream " << endl;
        return;
    }

    cout << "find stream " << endl;

    //デコーダーを見つける(video)
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    //デコーダーを見つける(audio)
    audioCodecCtx = pFormatCtx->streams[videoStream]->codec;
    audioCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if(pCodec == NULL)
    {
        cout << "not find codec " << endl;
    }

    cout << "codec find " << endl;

    //デコーダーを開く
    if(avcodec_open2(pCodecCtx,pCodec,NULL)<0)
    {
        cout << "not open codec " << endl;
    }
    cout << "codec open " << endl;

    PacketQueu *audioq = new PacketQueu;
    packet_queu_init(audioq);

    AVFrame* audioFrame = av_frame_alloc();

    mVideoState.aCodecCtx = audioCodecCtx;
    mVideoState.audioq =audioq;
    mVideoState.audioFrame =audioFrame;

    SDL_LockAudio();
    SDL_AudioSpec spec;
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audioCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = audioCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples =SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback =audio_callback;
    wanted_spec.userdata = &mVideoState;
    if(SDL_OpenAudio(&wanted_spec,&spec)< 0)
    {
        cout << "SDL Error " << endl;
    }
    SDL_UnlockAudio();
    SDL_PauseAudio(0);

    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();

    //デコードされたYUVデータをRGB32に変換するように変更
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    numBytes = avpicture_get_size(AV_PIX_FMT_RGB32,pCodecCtx->width, pCodecCtx->height);

    out_buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB,out_buffer,AV_PIX_FMT_RGB32,pCodecCtx->width, pCodecCtx->height);

    int y_size = pCodecCtx->width * pCodecCtx->height;

    packt = (AVPacket *) malloc(sizeof(AVPacket)); //パケット割り当て
    av_new_packet(packt,y_size); //パケットデータ割り当

    av_dump_format(pFormatCtx,0,file_path,0);//ビデオ情報を出力する

    int64_t start_time = av_gettime();
    int64_t pts = 0;
    int index = 0;

    while (1) {
        if(av_read_frame(pFormatCtx,packt) < 0)
        {
            break;
        }

        int64_t realTime = av_gettime()  -start_time;

        while(pts > realTime)
        {
            SDL_Delay(10);
            realTime = av_gettime() - start_time;
        }
        if(packt -> stream_index == videoStream)
        {
            ret = avcodec_decode_video2(pCodecCtx,pFrame,&got_picture,packt);

            if(packt->dts == AV_NOPTS_VALUE && pFrame->opaque && *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE)
            {
                pts = *(uint64_t *)pFrame->opaque;
            }
            else if(packt->dts != AV_NOPTS_VALUE)
            {
                pts = packt->dts;
            }
            else
            {
                pts = 0;
            }

            pts *= 1000000 * av_q2d(mVideoState.Video_st->time_base);
            pts = synchronize_video(&mVideoState,pFrame,pts);

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
                QImage image = tempImg.copy(); //画像がディスプレイに渡されます
                emit sig_GetOneFrame(image); //送信信号
            }
            av_free_packet(packt);
        }
        else if(packt->stream_index == audioStream)
        {
            packet_queu_put(mVideoState.audioq,packt);
        }
        else
        {
            av_free_packet(packt);
        }

        av_free_packet(packt);
    }
    av_free(out_buffer);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return;
}
