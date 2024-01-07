//
// Created by liuhongwei on 2021/12/7.
//

#include "VideoDecoder.h"

const char H264_NAL_START[] = { 0x00, 0x00, 0x00, 0x01 };

#define LOGD(...)
#define LOGI(...)
#define LOGE(...)
#define usleep(ms)    std::this_thread::sleep_for(std::chrono::milliseconds(ms));

VideoDecoder::VideoDecoder() {
    gSPSLen = 0;
    pSPS = nullptr;

    gPPSLen = 0;
    pPPS = nullptr;

    isFirstIDR = false;

    gFrameRate = 25;

    pFrameDataCallback = nullptr;
}

VideoDecoder::~VideoDecoder() {
}

void VideoDecoder::close() {
    isDecoding = false;
    decodeThread.join();

    if (pSPS != nullptr) {
        free(pSPS);
        pSPS = nullptr;
    }

    if (pPPS != nullptr) {
        free(pPPS);
        pPPS = nullptr;
    }

    if (pFrame != nullptr) {
        av_frame_free(&pFrame);
        LOGI("%s video Frame free", __FUNCTION__);
    }

    if (pVideoAVCodecCtx != nullptr) {
        avcodec_free_context(&pVideoAVCodecCtx);
        LOGI("%s video avcodec_free_context", __FUNCTION__);
    }
}

bool VideoDecoder::open(unsigned int frameRate, unsigned int profile, unsigned int level,
    char* sps, unsigned int spsLen, char* pps, unsigned int ppsLen) {
    gSPSLen = 0;
    pSPS = nullptr;

    gPPSLen = 0;
    pPPS = nullptr;

    LOGI("%s spsLen=%d ppsLen=%d", __FUNCTION__, spsLen, ppsLen);

    if (spsLen > 0) {
        pSPS = (char*)malloc(spsLen);
        if (nullptr == pSPS) {
            return false;
        }

        memcpy(pSPS, sps, spsLen);
        gSPSLen = spsLen;
    }

    if (ppsLen > 0) {
        pPPS = (char*)malloc(ppsLen);
        if (nullptr == pPPS) {
            free(pSPS);
            return false;
        }

        memcpy(pPPS, pps, ppsLen);
        gPPSLen = ppsLen;
    }

    isFirstIDR = false;

    if (frameRate > 0) {
        gFrameRate = frameRate;
    }

    int ret;
    auto dec = avcodec_find_decoder(AV_CODEC_ID_H264);
    LOGI("%s video decoder name: %s", __FUNCTION__, dec->name);
    pVideoAVCodecCtx = avcodec_alloc_context3(dec);

    if (pVideoAVCodecCtx == nullptr) {
        LOGE("%s VideoAVCodecCtx alloc failed", __FUNCTION__);
        return false;
    }

    AVCodecParameters* par = avcodec_parameters_alloc();
    if (par == nullptr) {
        LOGE("%s video AVCodecParameters alloc failed", __FUNCTION__);
        free(pSPS);
        free(pPPS);
        avcodec_free_context(&pVideoAVCodecCtx);
        return false;
    }

    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id = AV_CODEC_ID_H264;
    par->format = AV_PIX_FMT_YUV420P;//AV_PIX_FMT_NV12
    par->color_range = AVCOL_RANGE_JPEG;

    if (profile != 0) {
        par->profile = (int)profile;
    }

    if (level != 0) {
        par->level = (int)level;
    }

    avcodec_parameters_to_context(pVideoAVCodecCtx, par);
    avcodec_parameters_free(&par);

    LOGI("%s profile=%d level=%d", __FUNCTION__, profile, level);
    ret = avcodec_open2(pVideoAVCodecCtx, dec, nullptr);
    if (ret < 0) {
        LOGE("%s Can not open video encoder", __FUNCTION__);
        free(pSPS);
        free(pPPS);
        avcodec_free_context(&pVideoAVCodecCtx);
        return false;
    }
    LOGI("%s avcodec_open2 video SUCC", __FUNCTION__);
    pFrame = av_frame_alloc();
    if (pFrame == nullptr) {
        LOGE("%s video av_frame_alloc failed", __FUNCTION__);
        free(pSPS);
        free(pPPS);
        avcodec_free_context(&pVideoAVCodecCtx);
        return false;
    }

    isDecoding = true;
    decodeThread = std::thread(& decodeThread, nullptr, & VideoDecoder::_decode, (void*)this);
    if (ret != 0) {
        LOGE("video decode-thread create failed.\n");
        isDecoding = false;
        free(pSPS);
        free(pPPS);
        avcodec_free_context(&pVideoAVCodecCtx);
        av_frame_free(&pFrame);
        return false;
    }

    return true;
}

void VideoDecoder::decode() {
    int ret;
    unsigned sleepDelta = 1000000 / gFrameRate / 4;// 一帧视频的 1/4
    int NAL_START_LEN = 4;

    while (isDecoding) {

        AVPacket* pkt = av_packet_alloc();

        if (pkt == nullptr) {
            usleep(sleepDelta);
            continue;
        }

        if (pPacketQueue == nullptr) {
            av_packet_free(&pkt);
            usleep(sleepDelta);
            continue;
        }

        PACKET_STRUCT* packetStruct;
        bool isDone = pPacketQueue->Take(packetStruct);
        if (isDone && packetStruct != nullptr && packetStruct->data != nullptr &&
            packetStruct->data_size > 0) {
            //0x67:sps
            if (packetStruct->data[0] == 0x67) {
                if (gSPSLen <= 0) {
                    gSPSLen = packetStruct->data_size;
                    pSPS = (char*)malloc(gSPSLen);
                    if (nullptr == pSPS) {
                        av_packet_free(&pkt);
                        free(packetStruct->data);
                        free(packetStruct);

                        usleep(sleepDelta);
                        continue;
                    }
                    memcpy(pSPS, packetStruct->data, gSPSLen);
                    LOGI("%s get sps spsLen=%d", __FUNCTION__, gSPSLen);
                }

                av_packet_free(&pkt);
                free(packetStruct->data);
                free(packetStruct);

                continue;
            }
            //0x68:pps
            if (packetStruct->data[0] == 0x68) {
                if (gPPSLen <= 0) {
                    gPPSLen = packetStruct->data_size;
                    pPPS = (char*)malloc(gPPSLen);
                    if (nullptr == pPPS) {
                        av_packet_free(&pkt);
                        free(packetStruct->data);
                        free(packetStruct);

                        usleep(sleepDelta);
                        continue;
                    }
                    memcpy(pPPS, packetStruct->data, gPPSLen);
                    LOGI("%s get pps ppsLen=%d", __FUNCTION__, gPPSLen);
                }

                av_packet_free(&pkt);
                free(packetStruct->data);
                free(packetStruct);

                continue;
            }

            if (!isFirstIDR) {
                //0x65:IDR
                if (packetStruct->data[0] == 0x65) {
                    isFirstIDR = true;
                    LOGI("%s get first idr.", __FUNCTION__);
                }
                else {
                    av_packet_free(&pkt);
                    free(packetStruct->data);
                    free(packetStruct);

                    continue;
                }
            }

            if (packetStruct->data[0] == 0x65 && gSPSLen > 0 && gPPSLen > 0) {
                ret = av_new_packet(pkt, (int)(NAL_START_LEN + gSPSLen +
                    NAL_START_LEN + gPPSLen +
                    packetStruct->data_size + NAL_START_LEN));
            }
            else {
                ret = av_new_packet(pkt, packetStruct->data_size + NAL_START_LEN);
            }

            if (ret < 0) {
                av_packet_free(&pkt);
                free(packetStruct->data);
                free(packetStruct);

                usleep(sleepDelta);
                continue;
            }
        }
        else {
            av_packet_free(&pkt);
            usleep(sleepDelta);
            continue;
        }

        if (packetStruct->data[0] == 0x65 && gSPSLen > 0 && gPPSLen > 0) {
            int pos = 0;
            //复制 0x 00 00 00 01
            memcpy(pkt->data + pos, H264_NAL_START, NAL_START_LEN);
            pos += NAL_START_LEN;
            memcpy(pkt->data + pos, pSPS, gSPSLen);
            pos += (int)gSPSLen;

            memcpy(pkt->data + pos, H264_NAL_START, NAL_START_LEN);
            pos += NAL_START_LEN;
            memcpy(pkt->data + pos, pPPS, gPPSLen);
            pos += (int)gPPSLen;

            memcpy(pkt->data + pos, H264_NAL_START, NAL_START_LEN);
            pos += NAL_START_LEN;
            memcpy(pkt->data + pos, packetStruct->data, packetStruct->data_size);
        }
        else {
            memcpy(pkt->data, H264_NAL_START, NAL_START_LEN);
            memcpy(pkt->data + NAL_START_LEN, packetStruct->data, packetStruct->data_size);
        }

        pkt->pts = packetStruct->timestamp;
        pkt->dts = packetStruct->timestamp;

        free(packetStruct->data);
        free(packetStruct);
        /* send the packet for decoding */
        ret = avcodec_send_packet(pVideoAVCodecCtx, pkt);
        //LOGD("%s send the video packet for decoding pkt size=%d", __FUNCTION__, pkt->size);

        av_packet_unref(pkt);
        av_packet_free(&pkt);

        if (ret < 0) {
            LOGE("%s Error sending the video pkt to the decoder ret=%d", __FUNCTION__, ret);
            usleep(sleepDelta);
            continue;
        }
        else {
            // 编码和解码都是一样的，都是send 1次，然后receive多次, 直到AVERROR(EAGAIN)或者AVERROR_EOF
            while (ret >= 0) {
                ret = avcodec_receive_frame(pVideoAVCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    usleep(sleepDelta);
                    continue;
                }
                else if (ret < 0) {
                    LOGE("%s Error receive decoding video frame ret=%d", __FUNCTION__, ret);
                    usleep(sleepDelta);
                    continue;
                }

                std::lock_guard<std::mutex> lock(pFrameDataCallbackMutex);
                if (pFrameDataCallback != nullptr) {
                    // 解码固定为 AV_PIX_FMT_YUV420P
                    int planeNum = 3;
                    int yuvLens[planeNum];
                    yuvLens[0] = pFrame->linesize[0] * pFrame->height;
                    yuvLens[1] = pFrame->linesize[1] * pFrame->height / 2;
                    yuvLens[2] = pFrame->linesize[2] * pFrame->height / 2;
                    //LOGI("%s video onDataArrived", __FUNCTION__);
                    pFrameDataCallback->onDataArrived(StreamType::VIDEO,
                        (long long)pFrame->pts,
                        (char**)pFrame->data,
                        yuvLens,
                        planeNum,
                        -1,
                        -1,
                        pFrame->width,
                        pFrame->height);
                }

                av_frame_unref(pFrame);
            }
        }

    }
}