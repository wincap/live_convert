#ifndef __FFMPEG_TRANSCODE_H__  
#define __FFMPEG_TRANSCODE_H__  
#include <string.h> 
#include <stdio.h>
#include <vector>
#include <map>
#include <list>
 
using namespace std;
#define __STDC_CONSTANT_MACROS 


extern "C"  
{  
#include "libavformat/avformat.h"  
#include "libavformat/avio.h"  
#include "libavcodec/avcodec.h"  
#include "libswscale/swscale.h"  
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"  
#include "libavutil/mathematics.h"  
#include "libswresample/swresample.h"  
#include "libavutil/opt.h"  
#include "libavutil/channel_layout.h"  
#include "libavutil/samplefmt.h"  
#include "libavdevice/avdevice.h"  //摄像头所用  
#include "libavfilter/avfilter.h"  
#include "libavutil/error.h"  
#include "libavutil/mathematics.h"    
#include "libavutil/time.h"    
#include "libavutil/fifo.h"  
#include "libavutil/audio_fifo.h"   //这里是做分片时候重采样编码音频用的  
#include "inttypes.h"  
#include "stdint.h"  
};  
 
#pragma comment(lib,"avformat.lib")  
#pragma comment(lib,"avcodec.lib")  
#pragma comment(lib,"avdevice.lib")  
#pragma comment(lib,"avfilter.lib")  
#pragma comment(lib,"avutil.lib")  
#pragma comment(lib,"postproc.lib")  
#pragma comment(lib,"swresample.lib")  
#pragma comment(lib,"swscale.lib")  
 
//#define INPUTURL   "../in_stream/22.flv"   
//#define INPUTURL "../in_stream/闪电侠.The.Flash.S01E01.中英字幕.HDTVrip.624X352.mp4"  
//#define INPUTURL   "../in_stream/33.ts"   
//#define INPUTURL   "../in_stream/22mp4.mp4"   
//#define INPUTURL   "../in_stream/EYED0081.MOV"   
//#define INPUTURL   "../in_stream/李荣浩 - 李白.mp3"   
//#define INPUTURL   "../in_stream/avier1.mp4"   
//#define INPUTURL   "../in_stream/分歧者2预告片.mp4"   
//#define INPUTURL   "../in_stream/Class8简介.m4v"   
#define INPUTURL   "/data/video/instream/1.mp4"   
//#define INPUTURL   "../in_stream/44.mp3"  
//#define INPUTURL   "../in_stream/ceshi.mp4"  
//#define INPUTURL   "../in_stream/33.mp4"  
//#define INPUTURL   "../in_stream/father.avi"  
//#define INPUTURL   "../in_stream/22.flv"  
//#define INPUTURL   "../in_stream/西海情歌.wav"   
//#define INPUTURL   "../in_stream/Furious_7_2015_International_Trailer_2_5.1-1080p-HDTN.mp4"   
//#define INPUTURL   "../in_stream/Wildlife.wmv"   
//#define INPUTURL   "../in_stream/单身男女2.HD1280超清国语版.mp4"   
//#define INPUTURL     "rtmp://221.228.193.50:1935/live/teststream1"   
#define OUTPUTURL  "/data/video/outstream/output.flv" 
//http://10.69.112.96:8080/live/10flv/index.m3u8
#define OUTPUTURL10  "/data/video/outstream/10.flv"
//#define OUTPUTURL10   "rtmp://10.69.112.96:1936/live/10flv"
#define OUTPUTURL11  "/data/video/outstream/11.flv"  
//#define OUTPUTURL11   "rtmp://10.69.112.96:1936/live/11flv"
#define OUTPUTURL12  "/data/video/outstream/12.flv"  
//#define OUTPUTURL12   "rtmp://10.69.112.96:1936/live/12flv"
//#define OUTPUTURL    "rtmp://221.228.193.50:1935/live/zwg"  
//#define OUTPUTURL    "rtmp://221.228.193.50:1935/live/zwg"  
 
 
//样本枚举
enum AVSampleFormat_t   
{  
	AV_SAMPLE_FMT_NONE_t = -1,  
	AV_SAMPLE_FMT_U8_t,          ///< unsigned 8 bits  
	AV_SAMPLE_FMT_S16_t,         ///< signed 16 bits  
	AV_SAMPLE_FMT_S32_t,         ///< signed 32 bits  
	AV_SAMPLE_FMT_FLT_t,         ///< float  
	AV_SAMPLE_FMT_DBL_t,         ///< double  
 
	AV_SAMPLE_FMT_U8P_t,         ///< unsigned 8 bits, planar  
	AV_SAMPLE_FMT_S16P_t,        ///< signed 16 bits, planar  
	AV_SAMPLE_FMT_S32P_t,        ///< signed 32 bits, planar  
	AV_SAMPLE_FMT_FLTP_t,        ///< float, planar  
	AV_SAMPLE_FMT_DBLP_t,        ///< double, planar  
 
	AV_SAMPLE_FMT_NB_t           ///< Number of sample formats. DO NOT USE if linking dynamically  
};  
 
 
#define OUT_AUDIO_ID            0                                                 //packet 中的ID ，如果先加入音频 pocket 则音频是 0  视频是1，否则相反(影响add_out_stream顺序)  
#define OUT_VIDEO_ID            1  
 
//多路输出每一路的信息结构体
typedef struct Out_stream_info_t
{
	//user info
	int user_stream_id;                 //多路输出每一路的ID
	//video param  
	int m_dwWidth;  
	int m_dwHeight;  
	double m_dbFrameRate;               //帧率                                                    
	int m_video_codecID;  
	int m_video_pixelfromat; 
	int m_bit_rate;                     //码率
	int m_gop_size;  
	int m_max_b_frame;  
	int m_thread_count;                 //用cpu内核数目  
	//audio param  
	int m_dwChannelCount;               //声道  
	AVSampleFormat_t m_dwBitsPerSample; //样本  
	int m_dwFrequency;                  //采样率  
	int m_audio_codecID; 
 
	//ffmpeg out pram
	AVAudioFifo * m_audiofifo;          //音频存放pcm数据  
	int64_t m_first_audio_pts;          //第一帧的音频pts  
	int m_is_first_audio_pts;           //是否已经记录第一帧音频时间戳  
	AVFormatContext* m_ocodec ;         //输出流context  
	int m_writeheader_seccess;          //写头成功也就是写的头支持里面填写的音视频格式例如采样率等等
	AVStream* m_ovideo_st;                
	AVStream* m_oaudio_st;                
	AVCodec * m_audio_codec;  
	AVCodec * m_video_codec;  
	AVPacket m_pkt;     
	AVBitStreamFilterContext * m_vbsf_aac_adtstoasc;     //aac->adts to asc过滤器  
	struct SwsContext * m_img_convert_ctx_video;  
	int m_sws_flags;                    //差值算法,双三次 
	AVFrame * m_pout_video_frame;  
	AVFrame * m_pout_audio_frame;  
	SwrContext * m_swr_ctx;  
	char m_outurlname[256];             //输出的url地址
 
	Out_stream_info_t()
	{
		//user info
		user_stream_id = 0;             //多路输出每一路的ID
		//video param  
		m_dwWidth = 640;  
		m_dwHeight = 480;  
		m_dbFrameRate = 25;  //帧率                                                    
		m_video_codecID = (int)AV_CODEC_ID_H264;  
		m_video_pixelfromat = (int)AV_PIX_FMT_YUV420P;  
		m_bit_rate = 400000;                //码率
		m_gop_size = 12;  
		m_max_b_frame = 0;  
		m_thread_count = 2;                 //用cpu内核数目  
		//audio param  
		m_dwChannelCount = 2;               //声道  
		m_dwBitsPerSample = AV_SAMPLE_FMT_S16_t; //样本  
		m_dwFrequency = 44100;              //采样率  
		m_audio_codecID = (int)AV_CODEC_ID_AAC; 
 
		//ffmpeg out pram  
		m_audiofifo = NULL;                 //音频存放pcm数据  
		m_first_audio_pts = 0;              //第一帧的音频pts  
		m_is_first_audio_pts = 0;           //是否已经记录第一帧音频时间戳  
		m_ocodec = NULL;                    //输出流context 
		m_writeheader_seccess = 0; 
		m_ovideo_st = NULL;                
		m_oaudio_st = NULL;                
		m_audio_codec = NULL;  
		m_video_codec = NULL;  
		//m_pkt;     
		m_vbsf_aac_adtstoasc = NULL;        //aac->adts to asc过滤器  
		m_img_convert_ctx_video = NULL;  
		m_sws_flags = SWS_BICUBIC;          //差值算法,双三次  
		m_pout_video_frame = NULL;  
		m_pout_audio_frame = NULL; 
		m_swr_ctx = NULL;
		memset(m_outurlname,0,256);         //清零
	}
}Out_stream_info;
 
 
extern AVFormatContext* m_icodec;                         //输入流context  
extern int m_in_dbFrameRate;                              //输入流的帧率
extern int m_in_video_stream_idx;                         //输入流的视频序列号  
extern int m_in_audio_stream_idx;                         //输入流的音频序列号
extern int m_in_video_starttime;                          //输入流的视频起始时间
extern int m_in_audio_starttime;                          //输入流的音频起始时间
extern AVPacket m_in_pkt;                                 //读取输入文件packet
extern map<int,Out_stream_info*> m_list_out_stream_info;  //多路输出的list
 
//初始化demux
int ffmpeg_init_demux(char * inurlname,AVFormatContext ** iframe_c); 
//释放demux
int ffmpeg_uinit_demux(AVFormatContext * iframe_c);
//初始化mux:list,原始流只需要copy的
int ffmpeg_init_mux(map<int,Out_stream_info*> list_out_stream_info,int original_user_stream_id);  
//释放mux,原始流只需要copy的不用打开编码器
int ffmpeg_uinit_mux(map<int,Out_stream_info*> list_out_stream_info,int original_user_stream_id);  
 
//for mux copy 
AVStream * ffmpeg_add_out_stream(AVFormatContext* output_format_context,AVMediaType codec_type_t);   
//for codec  
AVStream * ffmpeg_add_out_stream2(Out_stream_info * out_stream_info,AVMediaType codec_type_t,AVCodec **codec);   
int ffmpeg_init_decode(int stream_type);  
int ffmpeg_init_code(int stream_type,AVStream* out_stream,AVCodec * out_codec);  
int ffmpeg_uinit_decode(int stream_type);  
int ffmpeg_uinit_code(int stream_type,AVStream* out_stream); 
//转码数据,原始流只需要copy的
int ffmpeg_transcode(int original_user_stream_id);
 
 
//下面是转码数据里面用的
int ffmpeg_perform_decode(int stream_type,AVFrame * picture);   
int ffmpeg_perform_code2(Out_stream_info * out_stream_info,int stream_type,AVFrame * picture);  //用于AVAudioFifo 
void ffmpeg_perform_yuv_conversion(Out_stream_info * out_stream_info,AVFrame * pinframe,AVFrame * poutframe); 
SwrContext * ffmpeg_init_pcm_resample(Out_stream_info * out_stream_info,AVFrame *in_frame, AVFrame *out_frame);  
int ffmpeg_preform_pcm_resample(Out_stream_info * out_stream_info,SwrContext * pSwrCtx,AVFrame *in_frame, AVFrame *out_frame); 
void ffmpeg_uinit_pcm_resample(SwrContext * swr_ctx,AVAudioFifo * audiofifo);
void ffmpeg_write_frame(Out_stream_info * out_stream_info,int ID,AVPacket pkt_t);        //copy
void ffmpeg_write_frame2(Out_stream_info * out_stream_info,int ID,AVPacket pkt_t);       //codec
 
#endif 
