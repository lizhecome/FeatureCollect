//
//  ImageUtil.hpp
//  FeatureCollect
//
//  Created by lizhe on 16/1/10.
//  Copyright © 2016年 TJGD. All rights reserved.
//

#ifndef ImageUtil_hpp
#define ImageUtil_hpp

#include <stdio.h>
#include <iostream>
#include "jpeglib.h"

/**
 *  保存帧数据为JPG图片
 *  该函数使用了jpeglib库进行图片压缩
 *
 *  @param pRGBBuffer RGB图像buffer
 *  @param nWidth     像素宽度
 *  @param nHeight    像素高度
 *
 *  @return jpeg图像
 */
inline std::pair<unsigned char*,unsigned long> SaveFrameToMEM(uint8_t *pRGBBuffer, int nWidth, int nHeight)
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
    
    return mRes;
}
#endif /* ImageUtil_hpp */
