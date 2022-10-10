/*
* Copyright 2017-2021 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/
#pragma once


extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}
#include "NvCodecUtils.h"

//---------------------------------------------------------------------------
//! \file FFmpegDemuxer.h 
//! \brief Provides functionality for stream demuxing
//!
//! This header file is used by Decode/Transcode apps to demux input video clips before decoding frames from it. 
//---------------------------------------------------------------------------

/**
* @brief libavformat wrapper class. Retrieves the elementary encoded stream from the container format.
*/
class FFmpegDemuxer {
private:
    AVFormatContext *fmtc = NULL;
    AVIOContext *avioc = NULL;
    AVPacket pkt, pktFiltered; /*!< AVPacket stores compressed data typically exported by demuxers and then passed as input to decoders */
    AVBSFContext *bsfc = NULL;

    int iVideoStream;
    bool bMp4H264, bMp4HEVC, bMp4MPEG4;
    AVCodecID eVideoCodec;
    AVPixelFormat eChromaFormat;
    int nWidth, nHeight, nBitDepth, nBPP, nChromaHeight;
    double timeBase = 0.0;
    int64_t userTimeScale = 0; 

    uint8_t *pDataWithHeader = NULL;

    unsigned int frameCount = 0;


public:

    /**
    *   @brief  Private constructor to initialize libavformat resources.
    *   @param  fmtc - Pointer to AVFormatContext allocated inside avformat_open_input()
    */
    FFmpegDemuxer(const char* szFilePath, bool printInfo = false, int64_t timeScale = 1000 /*Hz*/) {

		avformat_network_init();
		av_register_all();
		ck(avformat_open_input(&fmtc, szFilePath, NULL, NULL));
        if (!fmtc) {
            std::cout << "No AVFormatContext provided for " << szFilePath << ", Check if the filepath is correct" << std::endl;
            return;
        }

        if (printInfo) { 
            std::cout << "Media format: " << fmtc->iformat->long_name << " (" << fmtc->iformat->name << ")" << std::endl; 
        }

        ck(avformat_find_stream_info(fmtc, NULL));
        iVideoStream = av_find_best_stream(fmtc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (iVideoStream < 0) {
            std::cout << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " " << "Could not find stream in input file " << szFilePath;
            return;
        }

        eVideoCodec = fmtc->streams[iVideoStream]->codecpar->codec_id;
        nWidth = fmtc->streams[iVideoStream]->codecpar->width;
        nHeight = fmtc->streams[iVideoStream]->codecpar->height;
        eChromaFormat = (AVPixelFormat)fmtc->streams[iVideoStream]->codecpar->format;
        AVRational rTimeBase = fmtc->streams[iVideoStream]->time_base;
        timeBase = av_q2d(rTimeBase);
        userTimeScale = timeScale;

        // Set bit depth, chroma height, bits per pixel based on eChromaFormat of input
        switch (eChromaFormat)
        {
        case AV_PIX_FMT_YUV420P10LE:
        case AV_PIX_FMT_GRAY10LE:   // monochrome is treated as 420 with chroma filled with 0x0
            nBitDepth = 10;
            nChromaHeight = (nHeight + 1) >> 1;
            nBPP = 2;
            break;
        case AV_PIX_FMT_YUV420P12LE:
            nBitDepth = 12;
            nChromaHeight = (nHeight + 1) >> 1;
            nBPP = 2;
            break;
        case AV_PIX_FMT_YUV444P10LE:
            nBitDepth = 10;
            nChromaHeight = nHeight << 1;
            nBPP = 2;
            break;
        case AV_PIX_FMT_YUV444P12LE:
            nBitDepth = 12;
            nChromaHeight = nHeight << 1;
            nBPP = 2;
            break;
        case AV_PIX_FMT_YUV444P:
            nBitDepth = 8;
            nChromaHeight = nHeight << 1;
            nBPP = 1;
            break;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_GRAY8:      // monochrome is treated as 420 with chroma filled with 0x0
            nBitDepth = 8;
            nChromaHeight = (nHeight + 1) >> 1;
            nBPP = 1;
            break;
        default:
			std::cout << "ChromaFormat not recognized. Assuming 420";
            eChromaFormat = AV_PIX_FMT_YUV420P;
            nBitDepth = 8;
            nChromaHeight = (nHeight + 1) >> 1;
            nBPP = 1;
        }

        bMp4H264 = eVideoCodec == AV_CODEC_ID_H264 && (
                !strcmp(fmtc->iformat->long_name, "QuickTime / MOV") 
                || !strcmp(fmtc->iformat->long_name, "FLV (Flash Video)") 
                || !strcmp(fmtc->iformat->long_name, "Matroska / WebM")
            );
        bMp4HEVC = eVideoCodec == AV_CODEC_ID_HEVC && (
                !strcmp(fmtc->iformat->long_name, "QuickTime / MOV")
                || !strcmp(fmtc->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(fmtc->iformat->long_name, "Matroska / WebM")
            );

        bMp4MPEG4 = eVideoCodec == AV_CODEC_ID_MPEG4 && (
                !strcmp(fmtc->iformat->long_name, "QuickTime / MOV")
                || !strcmp(fmtc->iformat->long_name, "FLV (Flash Video)")
                || !strcmp(fmtc->iformat->long_name, "Matroska / WebM")
            );

        //Initialize packet fields with default values
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        av_init_packet(&pktFiltered);
        pktFiltered.data = NULL;
        pktFiltered.size = 0;

        // Initialize bitstream filter and its required resources
        if (bMp4H264) {
            const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
            if (!bsf) {
                std::cout << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " " << "av_bsf_get_by_name() failed";
                return;
            }
            ck(av_bsf_alloc(bsf, &bsfc));
            avcodec_parameters_copy(bsfc->par_in, fmtc->streams[iVideoStream]->codecpar);
            ck(av_bsf_init(bsfc));
        }
        if (bMp4HEVC) {
            const AVBitStreamFilter *bsf = av_bsf_get_by_name("hevc_mp4toannexb");
            if (!bsf) {
                std::cout << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " " << "av_bsf_get_by_name() failed";
                return;
            }
            ck(av_bsf_alloc(bsf, &bsfc));
            avcodec_parameters_copy(bsfc->par_in, fmtc->streams[iVideoStream]->codecpar);
            ck(av_bsf_init(bsfc));
        }
    }

    ~FFmpegDemuxer() {

        if (!fmtc) {
            return;
        }

        if (pkt.data) {
            av_packet_unref(&pkt);
        }
        if (pktFiltered.data) {
            av_packet_unref(&pktFiltered);
        }

        if (bsfc) {
            av_bsf_free(&bsfc);
        }

        avformat_close_input(&fmtc);

        if (avioc) {
            av_freep(&avioc->buffer);
            av_freep(&avioc);
        }

        if (pDataWithHeader) {
            av_free(pDataWithHeader);
        }
    }

    AVCodecID GetVideoCodec() {
        return eVideoCodec;
    }

    bool Demux(uint8_t **ppVideo, int *pnVideoBytes) {
        if (!fmtc) {
            return false;
        }

        *pnVideoBytes = 0;

        if (pkt.data) {
            av_packet_unref(&pkt);
        }

        int e = 0;
        while ((e = av_read_frame(fmtc, &pkt)) >= 0 && pkt.stream_index != iVideoStream) {
            av_packet_unref(&pkt);
        }
        if (e < 0) {
			if (e == AVERROR_EOF) {
				// reached end of file, start from the beginning
				avio_seek(fmtc->pb, 0, SEEK_SET);
				avformat_seek_file(fmtc, iVideoStream, 0, 0, fmtc->streams[iVideoStream]->duration, 0);
				while ((e = av_read_frame(fmtc, &pkt)) >= 0 && pkt.stream_index != iVideoStream) {
					av_packet_unref(&pkt);
				}
			}
			if (e < 0) {
				return false;
			}
        }

        if (bMp4H264 || bMp4HEVC) {
            if (pktFiltered.data) {
                av_packet_unref(&pktFiltered);
            }
            ck(av_bsf_send_packet(bsfc, &pkt));
            ck(av_bsf_receive_packet(bsfc, &pktFiltered));
            *ppVideo = pktFiltered.data;
            *pnVideoBytes = pktFiltered.size;
        } else {

            if (bMp4MPEG4 && (frameCount == 0)) {

                int extraDataSize = fmtc->streams[iVideoStream]->codecpar->extradata_size;

                if (extraDataSize > 0) {

                    // extradata contains start codes 00 00 01. Subtract its size
                    pDataWithHeader = (uint8_t *)av_malloc(extraDataSize + pkt.size - 3*sizeof(uint8_t));

                    if (!pDataWithHeader) {
                        std::cout << "FFmpeg error: " << __FILE__ << " " << __LINE__;
                        return false;
                    }

                    memcpy(pDataWithHeader, fmtc->streams[iVideoStream]->codecpar->extradata, extraDataSize);
                    memcpy(pDataWithHeader+extraDataSize, pkt.data+3, pkt.size - 3*sizeof(uint8_t));

                    *ppVideo = pDataWithHeader;
                    *pnVideoBytes = extraDataSize + pkt.size - 3*sizeof(uint8_t);
                }

            } else {
                *ppVideo = pkt.data;
                *pnVideoBytes = pkt.size;
            }
        }

        frameCount++;

        return true;
    }
};

inline cudaVideoCodec FFmpeg2NvCodecId(AVCodecID id) {
    switch (id) {
    case AV_CODEC_ID_MPEG1VIDEO : return cudaVideoCodec_MPEG1;
    case AV_CODEC_ID_MPEG2VIDEO : return cudaVideoCodec_MPEG2;
    case AV_CODEC_ID_MPEG4      : return cudaVideoCodec_MPEG4;
    case AV_CODEC_ID_WMV3       :
    case AV_CODEC_ID_VC1        : return cudaVideoCodec_VC1;
    case AV_CODEC_ID_H264       : return cudaVideoCodec_H264;
    case AV_CODEC_ID_HEVC       : return cudaVideoCodec_HEVC;
    case AV_CODEC_ID_VP8        : return cudaVideoCodec_VP8;
    case AV_CODEC_ID_VP9        : return cudaVideoCodec_VP9;
    case AV_CODEC_ID_MJPEG      : return cudaVideoCodec_JPEG;
    case AV_CODEC_ID_AV1        : return cudaVideoCodec_AV1;
    default                     : return cudaVideoCodec_NumCodecs;
    }
}


