//
// Created by liuhongwei on 2021/12/7.
//

#ifndef VIDEODECODER_H
#define VIDEODECODER_H
#include <thread>
#include <mutex>
#include <queue>

extern "C" {
    //±à½âÂë
#include "libavcodec/avcodec.h"
}

class VideoDecoder {

public:
    VideoDecoder();
    ~VideoDecoder();

    bool open(unsigned int frameRate, unsigned int profile, unsigned int level,
        char* sps, unsigned int spsLen, char* pps, unsigned int ppsLen);

    void close();

    void decode();

    static void* _decode(void* self) {
        static_cast<VideoDecoder*>(self)->decode();
        return nullptr;
    }

private:
    std::queue<AVFrame> pPacketQueue;
    AVCodecContext* pVideoAVCodecCtx;
    AVFrame* pFrame;

    bool volatile isDecoding;
    std::thread decodeThread;
    std::mutex pFrameDataCallbackMutex;

    char* pSPS;
    unsigned int volatile gSPSLen;
    char* pPPS;
    unsigned int volatile gPPSLen;

    bool volatile isFirstIDR;

    unsigned int gFrameRate;
};


#endif //VIDEODECODER_H#pragma once
