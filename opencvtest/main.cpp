#include <iostream>
#include <curl/curl.h>

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
using namespace cv;
char* filename = "udp://192.168.1.132:5000?overrun_nonfatal=0&fifo_size=50000000";


// 保存帧数据为JPG图片
// 该函数使用了jpeglib库进行图片压缩
std::pair<unsigned char*,unsigned long> SaveFrameToMEM(uint8_t *pRGBBuffer, int nWidth, int nHeight)
{
    std::pair<unsigned char*,unsigned long> mRes = std::make_pair( (unsigned char*)NULL, 0 );
    
    struct jpeg_compress_struct jcs;
    // 声明错误处理器，并赋值给jcs.err域
    struct jpeg_error_mgr jem;
    jcs.err = jpeg_std_error(&jem);
    jpeg_create_compress(&jcs);
    
    jpeg_mem_dest(&jcs, &mRes.first, &mRes.second);
    // 为图的宽和高，单位为像素
    jcs.image_width = nWidth;
    jcs.image_height = nHeight;
    // 在此为1,表示灰度图， 如果是彩色位图，则为3
    jcs.input_components = 3;
    //JCS_GRAYSCALE表示灰度图，JCS_RGB表示彩色图像
    jcs.in_color_space = JCS_RGB;
    
    jpeg_set_defaults(&jcs);
    jpeg_set_quality (&jcs, 80, TRUE);
    jpeg_start_compress(&jcs, TRUE);
    
    // 一行位图
    JSAMPROW row_pointer[1];
    //每一行的字节数,如果不是索引图,此处需要乘以3
    int row_stride = jcs.image_width * 3;
    
    // 对每一行进行压缩
    while (jcs.next_scanline < jcs.image_height)
    {
        row_pointer[0] = &(pRGBBuffer[jcs.next_scanline * row_stride]);
        jpeg_write_scanlines(&jcs, row_pointer, 1);
    }
    jpeg_finish_compress(&jcs);
    jpeg_destroy_compress(&jcs);
    
    //fclose(fp);
    return mRes;
}


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
                    unsigned char * picdata = NULL;
                    unsigned long picsize = 0L;
                    
                    std::pair<unsigned char*,unsigned long> pJpeg = SaveFrameToMEM(pFrameRGB->data[0], pCodecCtx->width, pCodecCtx->height);
                    std::ofstream of( "/Users/lizhe/Desktop/tes.jpg", ios::binary );
                    of.write( (char*)pJpeg.first, pJpeg.second );
                    of.close();
                    
                    CURL *curl = curl_easy_init();
                    
                    if (curl) {
                        
                        //headers = curl_slist_append(headers, "Content-Type: application/json");
                        struct curl_slist *headers = NULL;
                        headers = curl_slist_append(headers, "Accept: application/json");
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.137:4212/index/images/23");
                        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); /* !!! */
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pJpeg.second);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pJpeg.first); /* data goes here */
                        
                        curl_easy_perform(curl);
                        
                        //curl_slist_free_all(headers);
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



