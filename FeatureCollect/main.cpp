#include <iostream>
#include <curl/curl.h>
#include "ImageUtil.hpp"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
    //图像转换结构需要引入的头文件
#include "libswscale/swscale.h"
#include "jpeglib.h"
};
#include "ffmpegDecode.h"
#include <fstream>
using namespace std;

char const *filename = "udp://192.168.1.132:5000?overrun_nonfatal=0&fifo_size=50000000";

int main(int argc, char** argv)
{
    AVCodec *pCodec; //解码器指针
    AVCodecContext *pCodecCtx; //ffmpeg解码类的类成员
    AVFrame* pAvFrame; //多媒体指针，保存视频流的信息
    AVFormatContext* pFormatCtx;//保存视频流的信息
    
    av_register_all();//注册库中所有可用的文件格式和编码器
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    
    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0)
    {
        printf("Can't find the stream !\n");
    }
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)//查找流信息
    {
        printf("Can't find the stream information !\n");
    }
    
    int videoindex = -1;
    for(int i = 0; i < pFormatCtx->nb_streams; ++i)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }
    
    if(videoindex == -1)
    {
        printf("Don't find a video stream !\n");
        return -1;
    }
    
    pCodecCtx = pFormatCtx->streams[videoindex]->codec; //得到一个指向视频流上下文的指针
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        printf("Can't find the decoder !\n");//寻找解码器
        return -1;
    }
    if(avcodec_open2(pCodecCtx, pCodec, NULL))//打开解码器
    {
        printf("Can't open the decoder !\n");
    }
    
    pAvFrame = av_frame_alloc(); //分配帧存储空间
    AVFrame* pFrameRGB = av_frame_alloc();//存储解码后转换的RGB数据
    
    //保存BGR，opencv中是按BGR保存的
    int size = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    uint8_t *out_buffer = (uint8_t *)av_malloc(size);
    avpicture_fill((AVPicture*)pFrameRGB, out_buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    
    AVPacket* packet = (AVPacket*)malloc(sizeof(AVPacket));
    printf("-------输出文件信息---------\n");
    av_dump_format(pFormatCtx, 0, filename, 0);
    printf("--------------------------\n");

    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
    
    
    int ret;
    int got_picture;
    int count = 0;
    for (; ; ) {
        count++;
        if(av_read_frame(pFormatCtx, packet)>=0)
        {
            if (packet->stream_index == videoindex) {
                ret = avcodec_decode_video2(pCodecCtx, pAvFrame, &got_picture, packet);

                if (ret < 0) {
                    printf("Decode Error.(解码错误) \n");
                    return -1;
                }
                if (got_picture) {
                    //YUV to RGB
                    sws_scale(img_convert_ctx, (const uint8_t* const*)pAvFrame->data, pAvFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
                    
                    std::pair<unsigned char*,unsigned long> pJpeg = SaveFrameToMEM(pFrameRGB->data[0], pCodecCtx->width, pCodecCtx->height);
                    // 测试下,稍后删除
                    //std::ofstream of( "/Users/lizhe/Desktop/tes.jpg", ios::binary );
                    //of.write( (char*)pJpeg.first, pJpeg.second );
                    //of.close();
                    
                    CURL *curl = curl_easy_init();
                    
                    if (curl) {
                        struct curl_slist *headers = NULL;
                        headers = curl_slist_append(headers, "Accept: application/json");
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.137:4212/index/images/23");
                        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pJpeg.second);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pJpeg.first); /* data goes here */
                        
                        curl_easy_perform(curl);
                        
                        curl_slist_free_all(headers);
                        curl_easy_cleanup(curl);
                    }
                }
            }
            av_free_packet(packet);
        }else
        {
            break;
        }
    }

    av_free(out_buffer);
    av_free(pFrameRGB);
    av_free(pAvFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    system("pause");
    
    return 0;

}



