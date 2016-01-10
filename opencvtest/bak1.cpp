#include <iostream>

//#include <opencv/highgui.h>
//
//#include <opencv/cv.h>
#include <opencv2/opencv.hpp>
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include "SerializeMat.h"
#include <libmemcached/memcached.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
//图像转换结构需要引入的头文件
#include "libswscale/swscale.h"
};
#include "ffmpegDecode.h"
using namespace std;
using namespace cv;
char* filename = "udp://192.168.1.132:5000?overrun_nonfatal=0&fifo_size=50000000";

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
AVFrame* pFrameBGR = av_frame_alloc();//存储解码后转换的RGB数据

//保存BGR，opencv中是按BGR保存的
int size = avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
uint8_t *out_buffer = (uint8_t *)av_malloc(size);
avpicture_fill((AVPicture*)pFrameBGR, out_buffer, AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);

AVPacket* packet = (AVPacket*)malloc(sizeof(AVPacket));
printf("-------输出文件信息---------\n");
av_dump_format(pFormatCtx, 0, filename, 0);
printf("--------------------------\n");

struct SwsContext *img_convert_ctx;
img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

//opencv

cv::Mat pCvMat;
pCvMat.create(cv::Size(pCodecCtx->width,pCodecCtx->height), CV_8UC3);

int ret;
int got_picture;

cvNamedWindow("RGB",1);
for (; ; ) {
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
sws_scale(img_convert_ctx, (const uint8_t* const*)pAvFrame->data, pAvFrame->linesize, 0, pCodecCtx->height, pFrameBGR->data, pFrameBGR->linesize);

memcpy(pCvMat.data, out_buffer, size);

vector<KeyPoint> keypoints;

SurfFeatureDetector surf(2500.);
surf.detect(pCvMat,keypoints);
Mat descriptors;
SurfDescriptorExtractor Extractor;
Extractor.compute(pCvMat,keypoints,descriptors);


drawKeypoints(pCvMat,keypoints,pCvMat,Scalar(255,0,0),
DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
namedWindow("result");
//imshow("result",descriptors);
//
double t = getTickCount();//当前滴答数
std::ostringstream ossOut(ostringstream::out);
boost::archive::binary_oarchive oa(ossOut);
//boost::archive::text_oarchive oa(ofs);
oa << descriptors;
t = ((double)getTickCount() - t)/getTickFrequency();
cout<<"序列化用时："<<t<<"秒"<<endl;
//cout<<ossOut.str()<<endl;

memcached_st *memc;
memcached_return rc;
memcached_server_st *server;
time_t expiration;
uint32_t  flags;

memc = memcached_create(NULL);
server = memcached_server_list_append(NULL,"localhost",11211,&rc);
rc=memcached_server_push(memc,server);
memcached_server_list_free(server);

string key = "key";
string value = ossOut.str();
size_t value_length = value.length();
size_t key_length = key.length();

rc=memcached_set(memc,key.c_str(),key.length(),value.c_str(),value.length(),expiration,flags);
if(rc==MEMCACHED_SUCCESS)
{
cout<<"Save data:"<<value<<" sucessful!"<<endl;
}

//Get data
char* result = memcached_get(memc,key.c_str(),key_length,&value_length,&flags,&rc);
if(rc == MEMCACHED_SUCCESS)
{
cout<<"Get value:"<<result<<" sucessful!"<<endl;
}

Mat mtest;
std::istringstream ss(ossOut.str());
boost::archive::binary_iarchive ia(ss);
//boost::archive::text_iarchive ia(ifs);
ia >> mtest;
imshow("result",mtest);

cv::imwrite("/Users/lizhe/codebox/featurematch/pic/test.jpg", descriptors);
//imshow("RGB", pCvMat);

waitKey(1);
}
}
av_free_packet(packet);
}else
{
break;
}
}

av_free(out_buffer);
av_free(pFrameBGR);
av_free(pAvFrame);
avcodec_close(pCodecCtx);
avformat_close_input(&pFormatCtx);
cvDestroyWindow("RGB");
system("pause");

return 0;

}
