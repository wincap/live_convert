#include "ffmpeg_transcode.h"  
 
AVFormatContext* m_icodec = NULL;                            //输入流context  
int m_in_dbFrameRate = 0;                                    //输入流的帧率
int m_in_video_stream_idx = -1;                              //输入流的视频序列号  
int m_in_audio_stream_idx = -1;                              //输入流的音频序列号
int m_in_video_starttime = 0;                                //输入流的视频起始时间
int m_in_audio_starttime = 0;                                //输入流的音频起始时间
AVPacket m_in_pkt;                                           //读取输入文件packet
map<int,Out_stream_info*> m_list_out_stream_info ;           //多路输出的list
static FILE * pcm_file = NULL;                               //测试存储pcm用
 
int ffmpeg_init_demux(char * inurlname,AVFormatContext ** iframe_c)
{  
	int ret = 0;
	int i = 0;  
	ret = avformat_open_input(iframe_c, inurlname,NULL, NULL);  
	if (ret != 0)
	{
		printf("Call avformat_open_input function failed!\n");  
		return 0;  
	}  
	if (avformat_find_stream_info(*iframe_c,NULL) < 0)  
	{  
		printf("Call av_find_stream_info function failed!\n");  
		return 0;  
	}  
	//输出视频信息  
	av_dump_format(*iframe_c, -1, inurlname, 0);  
 
	//添加音频信息到输出context  
	for (i = 0; i < (*iframe_c)->nb_streams; i++)  
	{  
		if ((*iframe_c)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)  
		{  
			double FrameRate = (*iframe_c)->streams[i]->r_frame_rate.num /(double)(*iframe_c)->streams[i]->r_frame_rate.den;  
			m_in_dbFrameRate =(int)(FrameRate + 0.5);   
			m_in_video_stream_idx = i;  
			m_in_video_starttime = (*iframe_c)->streams[i]->start_time;
		}  
		else if ((*iframe_c)->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)  
		{  
			m_in_audio_stream_idx = i;  
			m_in_audio_starttime = (*iframe_c)->streams[i]->start_time;
		}  
	}  
 
	return 1;  
}  
 
int ffmpeg_uinit_demux(AVFormatContext * iframe_c)  
{  
	/* free the stream */  
	av_free(iframe_c);  
	return 1;  
}  
 
int ffmpeg_init_mux(map<int,Out_stream_info*> list_out_stream_info,int original_user_stream_id)
{
	int ret = 0;
	int i = 0;  
 
	if (list_out_stream_info.size()> 0)
	{
		map<int,Out_stream_info*> ::iterator result_all;
		Out_stream_info * out_stream_info_all = NULL;
		for (result_all = list_out_stream_info.begin();result_all != list_out_stream_info.end();)
		{
			out_stream_info_all = result_all->second;
			//如果存在输出
			if(out_stream_info_all)
			{
				/* allocate the output media context */  
				if (strstr(out_stream_info_all->m_outurlname,"rtmp"))
				{
					avformat_alloc_output_context2(&out_stream_info_all->m_ocodec, NULL,"flv", out_stream_info_all->m_outurlname);  
				}
				else
				{
					avformat_alloc_output_context2(&out_stream_info_all->m_ocodec, NULL,NULL, out_stream_info_all->m_outurlname);  
				} 
				if (!out_stream_info_all->m_ocodec)   
				{  
					return getchar();  
				}  
				AVOutputFormat* ofmt = NULL;  
				ofmt = out_stream_info_all->m_ocodec->oformat;  
 
				/* open the output file, if needed */  
				if (!(ofmt->flags & AVFMT_NOFILE))  
				{  
					if (avio_open(&out_stream_info_all->m_ocodec->pb, out_stream_info_all->m_outurlname, AVIO_FLAG_WRITE) < 0)  
					{  
						printf("Could not open '%s'\n", out_stream_info_all->m_outurlname);  
						return getchar();  
					}  
				} 
 
				//原始流只需要copy的
				if(out_stream_info_all->user_stream_id == original_user_stream_id)
				{
					//这里添加的时候AUDIO_ID/VIDEO_ID有影响  
					//添加音频信息到输出context  
					if(m_in_audio_stream_idx != -1)//如果输入存在音频  
					{   
						out_stream_info_all->m_oaudio_st = ffmpeg_add_out_stream(out_stream_info_all->m_ocodec, AVMEDIA_TYPE_AUDIO); 
 
						if ((strstr(out_stream_info_all->m_ocodec->oformat->name, "flv") != NULL) ||   
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "mp4") != NULL) ||   
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "mov") != NULL) ||  
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "3gp") != NULL))      
						{  
							if (out_stream_info_all->m_oaudio_st->codec->codec_id == AV_CODEC_ID_AAC)   
							{  
								out_stream_info_all->m_vbsf_aac_adtstoasc =  av_bitstream_filter_init("aac_adtstoasc");    
								if(out_stream_info_all->m_vbsf_aac_adtstoasc == NULL)    
								{    
									return -1;    
								}   
							}  
						}  
 
						//传给外层结构体
						out_stream_info_all->m_dwChannelCount = out_stream_info_all->m_oaudio_st->codec->channels;
						out_stream_info_all->m_dwBitsPerSample = (AVSampleFormat_t)out_stream_info_all->m_oaudio_st->codec->sample_fmt;
						out_stream_info_all->m_dwFrequency = out_stream_info_all->m_oaudio_st->codec->sample_rate;
						out_stream_info_all->m_audio_codecID = (int)out_stream_info_all->m_oaudio_st->codec->codec_id; 
					}  
 
					//添加视频信息到输出context  
					if (m_in_video_stream_idx != -1)//如果存在视频  
					{    
						out_stream_info_all->m_ovideo_st = ffmpeg_add_out_stream(out_stream_info_all->m_ocodec,AVMEDIA_TYPE_VIDEO);  
 
						//传给外层结构体
						out_stream_info_all->m_dwWidth = out_stream_info_all->m_ovideo_st->codec->width;
						out_stream_info_all->m_dwHeight = out_stream_info_all->m_ovideo_st->codec->height;
						out_stream_info_all->m_dbFrameRate = m_in_dbFrameRate;
						out_stream_info_all->m_video_codecID = (int)out_stream_info_all->m_ovideo_st->codec->codec_id;
						out_stream_info_all->m_video_pixelfromat = (int)out_stream_info_all->m_ovideo_st->codec->pix_fmt;
						out_stream_info_all->m_bit_rate = out_stream_info_all->m_ovideo_st->codec->bit_rate;
						out_stream_info_all->m_gop_size = out_stream_info_all->m_ovideo_st->codec->gop_size;
						out_stream_info_all->m_max_b_frame = out_stream_info_all->m_ovideo_st->codec->max_b_frames;
					}
				}
				else
				{
					//这里添加的时候AUDIO_ID/VIDEO_ID有影响  
					//添加音频信息到输出context  
					if(m_in_audio_stream_idx != -1)//如果输入存在音频  
					{   
						//添加流
						out_stream_info_all->m_oaudio_st = ffmpeg_add_out_stream2(out_stream_info_all, AVMEDIA_TYPE_AUDIO,
							&out_stream_info_all->m_audio_codec); 
 
						if ((strstr(out_stream_info_all->m_ocodec->oformat->name, "flv") != NULL) ||   
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "mp4") != NULL) ||   
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "mov") != NULL) ||  
							(strstr(out_stream_info_all->m_ocodec->oformat->name, "3gp") != NULL))      
						{  
							if (out_stream_info_all->m_oaudio_st->codec->codec_id == AV_CODEC_ID_AAC)   
							{  
								out_stream_info_all->m_vbsf_aac_adtstoasc =  av_bitstream_filter_init("aac_adtstoasc");    
								if(out_stream_info_all->m_vbsf_aac_adtstoasc == NULL)    
								{    
									return -1;    
								}   
							}  
						}  
 
						//编码初始化  
						ret = ffmpeg_init_code(OUT_AUDIO_ID,out_stream_info_all->m_oaudio_st,out_stream_info_all->m_audio_codec); 
					}  
 
					//添加视频信息到输出context  
					if (m_in_video_stream_idx != -1)//如果存在视频  
					{    
						//添加流
						out_stream_info_all->m_ovideo_st = ffmpeg_add_out_stream2(out_stream_info_all, AVMEDIA_TYPE_VIDEO,
							&out_stream_info_all->m_video_codec);
						//编码初始化  
						ret = ffmpeg_init_code(OUT_VIDEO_ID,out_stream_info_all->m_ovideo_st,out_stream_info_all->m_video_codec); 
					}  
				}
 
				//写头
				ret = avformat_write_header(out_stream_info_all->m_ocodec, NULL);  
				if (ret != 0)  
				{  
					out_stream_info_all->m_writeheader_seccess = 0;
					printf("Call avformat_write_header function failed.user_stream_id : %d\n",out_stream_info_all->user_stream_id);    
				} 
				else
				{
					out_stream_info_all->m_writeheader_seccess = 1;
				}
 
				//输出信息
				av_dump_format(out_stream_info_all->m_ocodec, 0, out_stream_info_all->m_outurlname, 1); 
			}   
			result_all ++;
		}
	}  
 
	//解码初始化  
	ret = ffmpeg_init_decode(OUT_AUDIO_ID); 
	//解码初始化  
	ret = ffmpeg_init_decode(OUT_VIDEO_ID);  
 
	ret = 1;
	return ret;
}
 
int ffmpeg_uinit_mux(map<int,Out_stream_info*> list_out_stream_info,int original_user_stream_id)
{
	int ret = 0;
	int i = 0;  
 
	if (m_list_out_stream_info.size()> 0)
	{
		map<int,Out_stream_info*> ::iterator result_all;
		Out_stream_info * out_stream_info_all = NULL;
		for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
		{
			out_stream_info_all = result_all->second;
			//如果存在输出
			if(out_stream_info_all && out_stream_info_all->m_writeheader_seccess == 1)
			{
				ret = av_write_trailer(out_stream_info_all->m_ocodec);  
				if (ret < 0)  
				{  
					printf("Call av_write_trailer function failed\n");  
					return getchar();
				}  
				if (out_stream_info_all->m_vbsf_aac_adtstoasc !=NULL)  
				{  
					av_bitstream_filter_close(out_stream_info_all->m_vbsf_aac_adtstoasc);   
					out_stream_info_all->m_vbsf_aac_adtstoasc = NULL;  
				}  
				av_dump_format(out_stream_info_all->m_ocodec, -1, out_stream_info_all->m_outurlname, 1);   
 
				if (m_in_video_stream_idx != -1)//如果存在视频  
				{  
					//原始流只需要copy的不用打开编码器
					if(out_stream_info_all->user_stream_id == original_user_stream_id)
					{
					}
					else
					{
						ffmpeg_uinit_decode(m_in_video_stream_idx);  
						ffmpeg_uinit_code(OUT_VIDEO_ID,out_stream_info_all->m_ovideo_st);
					}  
				}  
				if(m_in_audio_stream_idx != -1)//如果存在音频  
				{  
					//原始流只需要copy的不用打开编码器
					if(out_stream_info_all->user_stream_id == original_user_stream_id)
					{
					}
					else
					{
						ffmpeg_uinit_decode(m_in_audio_stream_idx);  
						ffmpeg_uinit_code(OUT_AUDIO_ID,out_stream_info_all->m_oaudio_st);  
					}  
				}  
				/* Free the streams. */  
				for (i = 0; i < out_stream_info_all->m_ocodec->nb_streams; i++)   
				{  
					av_freep(&out_stream_info_all->m_ocodec->streams[i]->codec);  
					av_freep(&out_stream_info_all->m_ocodec->streams[i]);  
				}  
				if (!(out_stream_info_all->m_ocodec->oformat->flags & AVFMT_NOFILE))  
				{  
					/* Close the output file. */  
					avio_close(out_stream_info_all->m_ocodec->pb);  
				}  
				av_free(out_stream_info_all->m_ocodec);  
 
				out_stream_info_all->m_writeheader_seccess = 0;
			}
			result_all ++;
		}
	}
	ret = 1;
	return ret;
}
 
AVStream * ffmpeg_add_out_stream(AVFormatContext* output_format_context,AVMediaType codec_type_t)
{

	int ret = 0;
	AVStream * in_stream = NULL;  
	AVStream * output_stream = NULL;  
	AVCodecContext* output_codec_context = NULL;  
 
	switch (codec_type_t)  
	{  
	case AVMEDIA_TYPE_AUDIO:  
		in_stream = m_icodec->streams[m_in_audio_stream_idx];  
		break;  
	case AVMEDIA_TYPE_VIDEO:  
		in_stream = m_icodec->streams[m_in_video_stream_idx];  
		break;  
	default:  
		break;  
	}  
 
	output_stream = avformat_new_stream(output_format_context,in_stream->codec->codec);  
	if (!output_stream)  
	{  
		return NULL;  
	}  
 
	output_stream->id = output_format_context->nb_streams - 1;  
	output_codec_context = output_stream->codec;  
 
	ret = avcodec_copy_context(output_stream->codec, in_stream->codec);  
	if (ret < 0)   
	{  
		printf("Failed to copy context from input to output stream codec context\n");  
		return NULL;  
	}
 
	//这个很重要，要么纯复用解复用，不做编解码写头会失败,  
	//另或者需要编解码如果不这样，生成的文件没有预览图，还有添加下面的header失败，置0之后会重新生成extradata  
	output_codec_context->codec_tag = 0;   
 
	//if(! strcmp( output_format_context-> oformat-> name,  "mp4" ) ||  
	//!strcmp (output_format_context ->oformat ->name , "mov" ) ||  
	//!strcmp (output_format_context ->oformat ->name , "3gp" ) ||  
	//!strcmp (output_format_context ->oformat ->name , "flv"))  
	
	if(AVFMT_GLOBALHEADER & output_format_context->oformat->flags)  
	{  
		output_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  
	}  
 
	return output_stream;  
}
 
AVStream * ffmpeg_add_out_stream2(Out_stream_info * out_stream_info,AVMediaType codec_type_t,AVCodec **codec)
{
	int ret = 0;
	AVCodecContext* output_codec_context = NULL;  
	AVStream * in_stream = NULL;  
	AVStream * output_stream = NULL;  
	AVCodecID codecID;  
 
	switch (codec_type_t)  
	{  
	case AVMEDIA_TYPE_AUDIO:  
		codecID = (AVCodecID)out_stream_info->m_audio_codecID;  
		in_stream = m_icodec->streams[m_in_audio_stream_idx];  
		break;  
	case AVMEDIA_TYPE_VIDEO:  
		codecID = (AVCodecID)out_stream_info->m_video_codecID;  
		in_stream = m_icodec->streams[m_in_video_stream_idx];  
		break;  
	default:  
		break;  
	}  
 
	/* find the encoder */  
        if (codec_type_t == AVMEDIA_TYPE_AUDIO) {
	    *codec = avcodec_find_encoder_by_name("libfdk_aac");
	} else {
	    *codec = avcodec_find_encoder(codecID);  
        }
	if (!(*codec))   
	{  
		return NULL;  
	}  
 
	output_stream = avformat_new_stream(out_stream_info->m_ocodec,*codec);  
	if (!output_stream)  
	{  
		return NULL;  
	}  
 
	output_stream->id = out_stream_info->m_ocodec->nb_streams - 1;  
	output_codec_context = output_stream->codec;  
 
	switch (codec_type_t)  
	{  
	case AVMEDIA_TYPE_AUDIO:  
		output_codec_context->codec_id = (AVCodecID)out_stream_info->m_audio_codecID;  
		output_codec_context->codec_type = codec_type_t;  
		AVRational time_base_in;
		time_base_in.num =1;
		if(!strcmp(out_stream_info->m_ocodec->oformat-> name,"mpegts" ))
		{
			time_base_in.den = 90*1000;
		}
		else
		{
			time_base_in.den = 1000;
		}
		output_stream->start_time = av_rescale_q_rnd(m_in_audio_starttime, in_stream->time_base, time_base_in, AV_ROUND_NEAR_INF);
		//output_codec_context->sample_rate = out_stream_info->m_dwFrequency;//m_icodec->streams[m_in_audio_stream_idx]->codec->sample_rate;//m_dwFrequency;  
		output_codec_context->sample_rate = m_icodec->streams[m_in_audio_stream_idx]->codec->sample_rate;//m_dwFrequency;  
		output_codec_context->channels  = out_stream_info->m_dwChannelCount;  
		output_codec_context->channel_layout = av_get_default_channel_layout(out_stream_info->m_dwChannelCount);	  
		//这个码率有些编码器不支持特别大，例如wav的码率是1411200 比aac大了10倍多  
		output_codec_context->bit_rate = 128000;//icodec->streams[audio_stream_idx]->codec->bit_rate;   
		output_codec_context->sample_fmt  = (*codec)->sample_fmts[0]; // (AVSampleFormat)out_stream_info->m_dwBitsPerSample; //样本  
		output_codec_context->block_align = 0;  
		break;  
	case AVMEDIA_TYPE_VIDEO:  
		AVRational r_frame_rate_t;  
		r_frame_rate_t.num = 100;  
		r_frame_rate_t.den = (int)(out_stream_info->m_dbFrameRate * 100);  
		output_codec_context->time_base.num = 1;  
		output_codec_context->time_base.den = out_stream_info->m_dbFrameRate;  
		output_stream->r_frame_rate.num = r_frame_rate_t.den;  
		output_stream->r_frame_rate.den = r_frame_rate_t.num;  
		output_codec_context->codec_id = (AVCodecID)out_stream_info->m_video_codecID; 
		output_codec_context->codec_type = codec_type_t;  
		if(!strcmp(out_stream_info->m_ocodec->oformat-> name,"mpegts" ))
		{
			time_base_in.den = 90*1000;
		}
		else
		{
			time_base_in.den = 1000;
		}
		output_stream->start_time = av_rescale_q_rnd(m_in_audio_starttime, in_stream->time_base, time_base_in, AV_ROUND_NEAR_INF);
		output_codec_context->pix_fmt = (AVPixelFormat)out_stream_info->m_video_pixelfromat;  
		output_codec_context->width = out_stream_info->m_dwWidth;  
		output_codec_context->height = out_stream_info->m_dwHeight;  
		output_codec_context->bit_rate = out_stream_info->m_bit_rate;  
		output_codec_context->gop_size  = out_stream_info->m_gop_size;         /* emit one intra frame every twelve frames at most */;  
		output_codec_context->max_b_frames = out_stream_info->m_max_b_frame;    //设置B帧最大数  
		output_codec_context->thread_count = out_stream_info->m_thread_count;  //线程数目  
		output_codec_context->me_range = 16;  
                output_codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
		output_codec_context->max_qdiff = 4;  
		output_codec_context->qmin = 20; //调节清晰度和编码速度 //这个值调节编码后输出数据量越大输出数据量越小，越大编码速度越快，清晰度越差  
		output_codec_context->qmax = 40; //调节清晰度和编码速度  
		output_codec_context->qcompress = 0.6;   
		//设置profile
		output_codec_context->profile = FF_PROFILE_H264_BASELINE;
		output_codec_context->level = 30;
		break;  
	default:  
		break;  
	}  
	//这个很重要，要么纯复用解复用，不做编解码写头会失败,  
	//另或者需要编解码如果不这样，生成的文件没有预览图，还有添加下面的header失败，置0之后会重新生成extradata  
	output_codec_context->codec_tag = 0;   
	//if(! strcmp( output_format_context-> oformat-> name,  "mp4" ) ||  
	//  !strcmp (output_format_context ->oformat ->name , "mov" ) ||  
	//  !strcmp (output_format_context ->oformat ->name , "3gp" ) ||  
	//  !strcmp (output_format_context ->oformat ->name , "flv" ))  
	if(AVFMT_GLOBALHEADER & out_stream_info->m_ocodec->oformat->flags)  
	{  
		output_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  
	}  
	return output_stream;  
}
 
int ffmpeg_init_decode(int stream_type)
{
	int ret = 0;
	AVCodec *pcodec = NULL;  
	AVCodecContext *cctext = NULL;  
 
	if (stream_type == OUT_AUDIO_ID)  
	{  
		cctext = m_icodec->streams[m_in_audio_stream_idx]->codec;  
		pcodec = avcodec_find_decoder(cctext->codec_id);  
		if (!pcodec)   
		{  
			return -1;  
		}  
	}  
	else if (stream_type == OUT_VIDEO_ID)  
	{  
		cctext = m_icodec->streams[m_in_video_stream_idx]->codec;  
		pcodec = avcodec_find_decoder(cctext->codec_id);  
		if (!pcodec)   
		{  
			return -1;  
		}  
	} 
 
	//实时解码关键看这句
	//cctext->flags |= CODEC_FLAG_LOW_DELAY;
	//cctext->flags2 |= CODEC_FLAG2_FAST;
 
	//打开解码器  
	ret = avcodec_open2(cctext, pcodec, NULL);   
	if (ret < 0)  
	{  
		printf("Could not open decoder\n");  
		return -1;  
	}  
 
	ret = 1; 
	return ret;
}
 
int ffmpeg_init_code(int stream_type,AVStream* out_stream,AVCodec * out_codec)
{
	int ret = 0;
	AVCodecContext *cctext = NULL;  
 
	if (stream_type == OUT_AUDIO_ID)  
	{  
		cctext = out_stream->codec; 
 
		//实时编码关键看这句
		av_opt_set(cctext->priv_data, "tune", "zerolatency", 0);
		//设置profile
		av_opt_set(cctext->priv_data, "profile", "baseline", AV_OPT_SEARCH_CHILDREN);
 
		//打开编码器  
		ret = avcodec_open2(cctext, out_codec, NULL);   
		if (ret < 0)  
		{  
			printf("Could not open encoder\n");  
			return 0;  
		}  
	}  
	else if (stream_type == OUT_VIDEO_ID)  
	{  
		cctext = out_stream->codec; 
 
		//实时编码关键看这句
		av_opt_set(cctext->priv_data, "tune", "zerolatency", 0);
		//设置profile
		av_opt_set(cctext->priv_data, "profile", "baseline", AV_OPT_SEARCH_CHILDREN);
 
		//打开编码器  
		ret = avcodec_open2(cctext, out_codec, NULL);   
		if (ret < 0)  
		{  
			printf("Could not open encoder\n");  
			return -1;  
		}  
	}  
	ret = 1;
 
	return ret;
}
 
int ffmpeg_uinit_decode(int stream_type)
{
	int ret = 0;
 
	AVCodecContext *cctext = NULL;  
 
	if (stream_type == m_in_audio_stream_idx)  
	{  
		cctext = m_icodec->streams[m_in_audio_stream_idx]->codec;   
	}  
	else if (stream_type == m_in_video_stream_idx)  
	{  
		cctext = m_icodec->streams[m_in_video_stream_idx]->codec;   
	}  
	avcodec_close(cctext);  
 
	ret = 1;
	return ret;
}
 
int ffmpeg_uinit_code(int stream_type,AVStream* out_stream)
{
	int ret = 0;
	AVCodecContext *cctext = NULL;  
 
	if (stream_type == OUT_AUDIO_ID)  
	{  
		cctext = out_stream->codec;   
	}  
	else if (stream_type == OUT_VIDEO_ID)  
	{  
		cctext = out_stream->codec;   
	}  
	avcodec_close(cctext);  
	
	ret = 1; 
	return ret;
}
 
int ffmpeg_perform_decode(int stream_type,AVFrame * picture)
{
	int ret = 0;
	AVCodecContext *cctext = NULL;  
	int frameFinished = 0 ;   
 
	if (stream_type == OUT_AUDIO_ID)  
	{  
		cctext = m_icodec->streams[m_in_audio_stream_idx]->codec;
 
		m_in_pkt.pts = av_rescale_q_rnd(m_in_pkt.pts, m_icodec->streams[m_in_audio_stream_idx]->time_base, cctext->time_base, AV_ROUND_NEAR_INF);  
		m_in_pkt.dts = av_rescale_q_rnd(m_in_pkt.dts, m_icodec->streams[m_in_audio_stream_idx]->time_base, cctext->time_base, AV_ROUND_NEAR_INF);
 
		avcodec_decode_audio4(cctext,picture,&frameFinished,&m_in_pkt);  
		if(frameFinished)  
		{  
			return 0;  
		}  
	}  
	else if (stream_type == OUT_VIDEO_ID)  
	{  
		cctext = m_icodec->streams[m_in_video_stream_idx]->codec; 
 
		m_in_pkt.pts = av_rescale_q_rnd(m_in_pkt.pts, m_icodec->streams[m_in_video_stream_idx]->time_base, cctext->time_base, AV_ROUND_NEAR_INF);  
		m_in_pkt.dts = av_rescale_q_rnd(m_in_pkt.dts, m_icodec->streams[m_in_video_stream_idx]->time_base, cctext->time_base, AV_ROUND_NEAR_INF);
 
		avcodec_decode_video2(cctext,picture,&frameFinished,&m_in_pkt);  
		if(frameFinished)  
		{  
			return 0;  
		}  
	}   
	ret = 1;
	return ret;
}
 
int ffmpeg_perform_code2(Out_stream_info * out_stream_info,int stream_type,AVFrame * picture)
{
	int ret = 0;
	AVCodecContext *cctext = NULL;  
	AVPacket pkt_t;  
	av_init_packet(&pkt_t);  
	pkt_t.data = NULL; // packet data will be allocated by the encoder  
	pkt_t.size = 0;  
	int frameFinished = 0 ;  
 
	if (stream_type == OUT_AUDIO_ID)  
	{  
		cctext = out_stream_info->m_oaudio_st->codec;
 
		int64_t pts_t = picture->pts;  
		int duration_t = 0;  //要编码的一帧持续时间
 
		duration_t = (double)cctext->frame_size * (out_stream_info->m_oaudio_st->codec->time_base.den /out_stream_info->m_oaudio_st->codec->time_base.num)/   
			out_stream_info->m_oaudio_st->codec->sample_rate;  
 
		AVFrame * pFrameResample = av_frame_alloc();  
		pFrameResample = av_frame_alloc();  
 
		pFrameResample->nb_samples     = cctext->frame_size;  
		pFrameResample->channel_layout = cctext->channel_layout;  
		pFrameResample->channels = cctext->channels;  
		pFrameResample->format         = cctext->sample_fmt;  
		pFrameResample->sample_rate    = cctext->sample_rate;  
		int error = 0;  
		if ((error = av_frame_get_buffer(pFrameResample, 0)) < 0)  
		{  
			av_frame_free(&pFrameResample);  
			return error;  
		}  
		while (av_audio_fifo_size(out_stream_info->m_audiofifo) >= pFrameResample->nb_samples) //取出写入的未读的包  
		{  
			av_audio_fifo_read(out_stream_info->m_audiofifo,(void **)pFrameResample->data,pFrameResample->nb_samples);  
 
			if(out_stream_info->m_is_first_audio_pts == 0)  
			{  
				out_stream_info->m_first_audio_pts = pts_t; 
				out_stream_info->m_is_first_audio_pts = 1;  
			}  
			pFrameResample->pts = out_stream_info->m_first_audio_pts; 
			out_stream_info->m_first_audio_pts += duration_t;  
 
			pFrameResample->pkt_pts = pFrameResample->pts;
			pFrameResample->pkt_dts = pFrameResample->pts;
			ret = avcodec_encode_audio2(cctext,&pkt_t,pFrameResample,&frameFinished);  
			if (ret>=0 && frameFinished)  
			{  
				ffmpeg_write_frame2(out_stream_info,OUT_AUDIO_ID,pkt_t); 
				av_free_packet(&pkt_t);  
			}  
		} 
 
		if (pFrameResample)  
		{  
			av_frame_free(&pFrameResample);  
			pFrameResample = NULL;  
		}  
	}  
	else if (stream_type == OUT_VIDEO_ID)  
	{  
		cctext = out_stream_info->m_ovideo_st->codec;  
		picture->pts = av_rescale_q_rnd(picture->pts, m_icodec->streams[m_in_video_stream_idx]->codec->time_base, out_stream_info->m_ovideo_st->codec->time_base, AV_ROUND_NEAR_INF);  
		picture->pkt_pts = picture->pts;
		picture->pkt_dts = picture->pts;
 
		avcodec_encode_video2(cctext,&pkt_t,picture,&frameFinished);  
		picture->pts++;  
		if (frameFinished)  
		{  
			ffmpeg_write_frame2(out_stream_info,OUT_VIDEO_ID,pkt_t);  
			av_free_packet(&pkt_t);  
		}   
	}  
	ret = 1;
	return ret;
}
 
void ffmpeg_perform_yuv_conversion(Out_stream_info * out_stream_info,AVFrame * pinframe,AVFrame * poutframe)
{
	int ret = 0;
 
	//设置转换context  
	if (out_stream_info->m_img_convert_ctx_video == NULL)     
	{  
		out_stream_info->m_img_convert_ctx_video = sws_getContext(
			m_icodec->streams[m_in_video_stream_idx]->codec->width, m_icodec->streams[m_in_video_stream_idx]->codec->height,   
			m_icodec->streams[m_in_video_stream_idx]->codec->pix_fmt,  
			out_stream_info->m_dwWidth, out_stream_info->m_dwHeight,  
			(AVPixelFormat)out_stream_info->m_video_pixelfromat,  
			out_stream_info->m_sws_flags, NULL, NULL, NULL);  
		if (out_stream_info->m_img_convert_ctx_video == NULL)  
		{  
			printf("Cannot initialize the conversion context\n");  
		}  
	}  
	//开始转换  
	sws_scale(out_stream_info->m_img_convert_ctx_video, pinframe->data, pinframe->linesize,           
		0, m_icodec->streams[m_in_video_stream_idx]->codec->height, poutframe->data, poutframe->linesize);  
	poutframe->pkt_pts = pinframe->pkt_pts;  
	poutframe->pkt_dts = pinframe->pkt_dts;  
	//有时pkt_pts和pkt_dts不同，并且pkt_pts是编码前的dts,这里要给avframe传入pkt_dts而不能用pkt_pts  
	//poutframe->pts = poutframe->pkt_pts;  
	poutframe->pts = pinframe->pkt_dts;  
 
	poutframe->key_frame = pinframe->key_frame;
	if (poutframe->key_frame == 1)
	{
		poutframe->pict_type = AV_PICTURE_TYPE_I;
	}
	else
	{
		poutframe->pict_type = AV_PICTURE_TYPE_P;
	}
 
	ret = 1;
	return;
}
 
SwrContext * ffmpeg_init_pcm_resample(Out_stream_info * out_stream_info,AVFrame *in_frame, AVFrame *out_frame)
{
	SwrContext * swr_ctx = NULL;  
	swr_ctx = swr_alloc();  
	if (!swr_ctx)  
	{  
		printf("swr_alloc error \n");  
		return NULL;  
	}  
	AVCodecContext * audio_dec_ctx = m_icodec->streams[m_in_audio_stream_idx]->codec;  
	AVSampleFormat sample_fmt;  
	sample_fmt = (AVSampleFormat)out_stream_info->m_dwBitsPerSample; //样本  
	int out_channel_layout = av_get_default_channel_layout(out_stream_info->m_dwChannelCount);
	if (audio_dec_ctx->channel_layout == 0)  
	{  
		audio_dec_ctx->channel_layout = av_get_default_channel_layout(m_icodec->streams[m_in_audio_stream_idx]->codec->channels);  
	}  
	/* set options */  
	av_opt_set_int(swr_ctx, "in_channel_layout",    audio_dec_ctx->channel_layout, 0);  
	av_opt_set_int(swr_ctx, "in_sample_rate",       audio_dec_ctx->sample_rate, 0);  
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_dec_ctx->sample_fmt, 0);  
	av_opt_set_int(swr_ctx, "out_channel_layout",   out_channel_layout, 0);   
	av_opt_set_int(swr_ctx, "out_sample_rate",       out_stream_info->m_dwFrequency, 0);  
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", sample_fmt, 0);  
	swr_init(swr_ctx);  
 
	int64_t src_nb_samples = in_frame->nb_samples; 
	//计算输出的samples 和采样率有关 例如：48000转44100，samples则是从1152转为1059，除法
	out_frame->nb_samples = av_rescale_rnd(src_nb_samples, out_stream_info->m_dwFrequency, audio_dec_ctx->sample_rate, AV_ROUND_UP);
 
	int ret = av_samples_alloc(out_frame->data, &out_frame->linesize[0],   
		out_stream_info->m_dwChannelCount, out_frame->nb_samples,out_stream_info->m_oaudio_st->codec->sample_fmt,1);  
	if (ret < 0)  
	{  
		return NULL;  
	}  
 
	out_stream_info->m_audiofifo  = av_audio_fifo_alloc(out_stream_info->m_oaudio_st->codec->sample_fmt, out_stream_info->m_oaudio_st->codec->channels,  
		out_frame->nb_samples);   
 
	return swr_ctx;  
}
 
int ffmpeg_preform_pcm_resample(Out_stream_info * out_stream_info,SwrContext * pSwrCtx,AVFrame *in_frame, AVFrame *out_frame)
{
	int ret = 0;
	int samples_out_per_size = 0;              //转换之后的samples大小
  
	if (pSwrCtx != NULL)   
	{  
		//这里注意下samples_out_per_size这个值和 out_frame->nb_samples这个值有时候不一样，ffmpeg里面做了策略不是问题。
		samples_out_per_size = swr_convert(pSwrCtx, out_frame->data, out_frame->nb_samples,   
			(const uint8_t**)in_frame->data, in_frame->nb_samples);  
		if (samples_out_per_size < 0)  
		{  
			return -1;  
		}  
 
		AVCodecContext * audio_dec_ctx = m_icodec->streams[m_in_audio_stream_idx]->codec; 
 
		int buffersize_in = av_samples_get_buffer_size(&in_frame->linesize[0],audio_dec_ctx->channels,  
			in_frame->nb_samples, audio_dec_ctx->sample_fmt, 1);
 
		//修改分包内存  
		int buffersize_out = av_samples_get_buffer_size(&out_frame->linesize[0], out_stream_info->m_oaudio_st->codec->channels,  
			samples_out_per_size, out_stream_info->m_oaudio_st->codec->sample_fmt, 1); 
 
		int fifo_size = av_audio_fifo_size(out_stream_info->m_audiofifo);  
		fifo_size = av_audio_fifo_realloc(out_stream_info->m_audiofifo, av_audio_fifo_size(out_stream_info->m_audiofifo) + out_frame->nb_samples);  
		av_audio_fifo_write(out_stream_info->m_audiofifo,(void **)out_frame->data,samples_out_per_size);  
		fifo_size = av_audio_fifo_size(out_stream_info->m_audiofifo); 
 
		out_frame->pkt_pts = in_frame->pkt_pts;  
		out_frame->pkt_dts = in_frame->pkt_dts;  
		//有时pkt_pts和pkt_dts不同，并且pkt_pts是编码前的dts,这里要给avframe传入pkt_dts而不能用pkt_pts  
		//out_frame->pts = out_frame->pkt_pts;  
		out_frame->pts = in_frame->pkt_dts;  
 
		//测试用
		if (out_stream_info->user_stream_id ==11)
		{
			if (pcm_file == NULL)
			{
				pcm_file = fopen("11.pcm","wb");
			}
			int wtiresize = fwrite(out_frame->data[0],buffersize_out,1, pcm_file);
			fflush(pcm_file);
		}
	}  
	ret = 1;
	return ret;
}
 
void ffmpeg_uinit_pcm_resample(SwrContext * swr_ctx,AVAudioFifo * audiofifo)
{ 
	if (swr_ctx)  
	{  
		swr_free(&swr_ctx);  
		swr_ctx = NULL;  
	}  
	if(audiofifo)  
	{  
		av_audio_fifo_free(audiofifo);  
		audiofifo = NULL;  
	}     
}
 
void ffmpeg_write_frame(Out_stream_info * out_stream_info,int ID,AVPacket pkt_t)
{
	int64_t pts = 0, dts = 0;  
	int ret = -1;  
 
	if(ID == OUT_VIDEO_ID)  
	{  
		AVPacket videopacket_t;  
		av_init_packet(&videopacket_t);  
 
		videopacket_t.pts = av_rescale_q_rnd(pkt_t.pts, m_icodec->streams[m_in_video_stream_idx]->time_base, out_stream_info->m_ovideo_st->time_base, AV_ROUND_NEAR_INF);  
		videopacket_t.dts = av_rescale_q_rnd(pkt_t.dts, m_icodec->streams[m_in_video_stream_idx]->time_base, out_stream_info->m_ovideo_st->time_base, AV_ROUND_NEAR_INF);  
		videopacket_t.duration = av_rescale_q(pkt_t.duration,m_icodec->streams[m_in_video_stream_idx]->time_base, out_stream_info->m_ovideo_st->time_base);  
		videopacket_t.flags = pkt_t.flags;  
		videopacket_t.stream_index = OUT_VIDEO_ID; //这里add_out_stream顺序有影响  
		videopacket_t.data = pkt_t.data;  
		videopacket_t.size = pkt_t.size;  
		videopacket_t.pos = -1;
		ret = av_interleaved_write_frame(out_stream_info->m_ocodec, &videopacket_t);  
		if (ret != 0)  
		{  
			printf("error av_interleaved_write_frame _ video\n");  
		}  
		printf("video\n");  
	}  
	else if(ID == OUT_AUDIO_ID)  
	{  
		AVPacket audiopacket_t;  
		av_init_packet(&audiopacket_t);  
 
		audiopacket_t.pts = av_rescale_q_rnd(pkt_t.pts, m_icodec->streams[m_in_audio_stream_idx]->time_base, out_stream_info->m_oaudio_st->time_base, AV_ROUND_NEAR_INF);  
		audiopacket_t.dts = av_rescale_q_rnd(pkt_t.dts, m_icodec->streams[m_in_audio_stream_idx]->time_base, out_stream_info->m_oaudio_st->time_base, AV_ROUND_NEAR_INF);  
		audiopacket_t.duration = av_rescale_q(pkt_t.duration,m_icodec->streams[m_in_audio_stream_idx]->time_base, out_stream_info->m_oaudio_st->time_base); 
		audiopacket_t.flags = pkt_t.flags;  
		audiopacket_t.stream_index = OUT_AUDIO_ID; //这里add_out_stream顺序有影响  
		audiopacket_t.data = pkt_t.data;  
		audiopacket_t.size = pkt_t.size; 
		audiopacket_t.pos = -1;
 
		//添加过滤器  
		if(! strcmp(out_stream_info->m_ocodec->oformat-> name,  "mp4" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "mov" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "3gp" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "flv" ))  
		{  
			if (out_stream_info->m_oaudio_st->codec->codec_id == AV_CODEC_ID_AAC)  
			{  
				if (out_stream_info->m_vbsf_aac_adtstoasc != NULL)  
				{  
					AVPacket filteredPacket = audiopacket_t;   
					int a = av_bitstream_filter_filter(out_stream_info->m_vbsf_aac_adtstoasc,                                             
						out_stream_info->m_oaudio_st->codec, NULL,&filteredPacket.data, &filteredPacket.size,  
						audiopacket_t.data, audiopacket_t.size, audiopacket_t.flags & AV_PKT_FLAG_KEY);   
					if (a >  0)               
					{                  
						av_free_packet(&audiopacket_t);   
						//filteredPacket.destruct = av_destruct_packet;    
						//filteredPacket.destruct = NULL;    
						audiopacket_t = filteredPacket;               
					}     
					else if (a == 0)  
					{  
						audiopacket_t = filteredPacket;     
					}  
					else if (a < 0)              
					{                  
						fprintf(stderr, "%s failed for stream %d, codec %s",  
							out_stream_info->m_vbsf_aac_adtstoasc->filter->name,audiopacket_t.stream_index,out_stream_info->m_oaudio_st->codec->codec ?  out_stream_info->m_oaudio_st->codec->codec->name : "copy");  
						av_free_packet(&audiopacket_t);     
 
					}  
				}  
			}  
		}  
		ret = av_interleaved_write_frame(out_stream_info->m_ocodec, &audiopacket_t);  
		if (ret != 0)  
		{  
			printf("error av_interleaved_write_frame _ audio\n");  
		}  
		printf("audio\n");  
	} 
}
 
void ffmpeg_write_frame2(Out_stream_info * out_stream_info,int ID,AVPacket pkt_t)
{
	int64_t pts = 0, dts = 0;  
	int ret = -1;  
 
	if(ID == OUT_VIDEO_ID)  
	{  
		AVPacket videopacket_t;  
		av_init_packet(&videopacket_t);  
 
		videopacket_t.pts = av_rescale_q_rnd(pkt_t.pts, out_stream_info->m_ovideo_st->codec->time_base, out_stream_info->m_ovideo_st->time_base, AV_ROUND_NEAR_INF);  
		videopacket_t.dts = av_rescale_q_rnd(pkt_t.dts, out_stream_info->m_ovideo_st->codec->time_base, out_stream_info->m_ovideo_st->time_base, AV_ROUND_NEAR_INF);  
		videopacket_t.duration = av_rescale_q(pkt_t.duration,out_stream_info->m_ovideo_st->codec->time_base, out_stream_info->m_ovideo_st->time_base);  
		videopacket_t.flags = pkt_t.flags;  
		videopacket_t.stream_index = OUT_VIDEO_ID; //这里add_out_stream顺序有影响  
		videopacket_t.data = pkt_t.data;  
		videopacket_t.size = pkt_t.size;  
		ret = av_interleaved_write_frame(out_stream_info->m_ocodec, &videopacket_t);  
		if (ret != 0)  
		{  
			printf("error av_interleaved_write_frame _ video\n");  
		}  
		printf("video\n"); 
	}  
	else if(ID == OUT_AUDIO_ID)  
	{  
		AVPacket audiopacket_t;  
		av_init_packet(&audiopacket_t);  
 
		audiopacket_t.pts = av_rescale_q_rnd(pkt_t.pts, out_stream_info->m_oaudio_st->codec->time_base, out_stream_info->m_oaudio_st->time_base, AV_ROUND_NEAR_INF);  
		audiopacket_t.dts = av_rescale_q_rnd(pkt_t.dts, out_stream_info->m_oaudio_st->codec->time_base, out_stream_info->m_oaudio_st->time_base, AV_ROUND_NEAR_INF);  
		audiopacket_t.duration = av_rescale_q(pkt_t.duration,out_stream_info->m_oaudio_st->codec->time_base, out_stream_info->m_oaudio_st->time_base); 
		audiopacket_t.flags = pkt_t.flags;  
		audiopacket_t.stream_index = OUT_AUDIO_ID; //这里add_out_stream顺序有影响  
		audiopacket_t.data = pkt_t.data;  
		audiopacket_t.size = pkt_t.size;  
 
		//添加过滤器  
		if(! strcmp(out_stream_info->m_ocodec->oformat-> name,  "mp4" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "mov" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "3gp" ) ||  
			!strcmp (out_stream_info->m_ocodec ->oformat ->name , "flv" ))  
		{  
			if (out_stream_info->m_oaudio_st->codec->codec_id == AV_CODEC_ID_AAC)  
			{  
				if (out_stream_info->m_vbsf_aac_adtstoasc != NULL)  
				{  
					AVPacket filteredPacket = audiopacket_t;   
					int a = av_bitstream_filter_filter(out_stream_info->m_vbsf_aac_adtstoasc,                                             
						out_stream_info->m_oaudio_st->codec, NULL,&filteredPacket.data, &filteredPacket.size,  
						audiopacket_t.data, audiopacket_t.size, audiopacket_t.flags & AV_PKT_FLAG_KEY);   
					if (a >  0)               
					{                  
						av_free_packet(&audiopacket_t);   
					//	filteredPacket.destruct = NULL;    
						audiopacket_t = filteredPacket;               
					}     
					else if (a == 0)  
					{  
						audiopacket_t = filteredPacket;     
					}  
					else if (a < 0)              
					{                  
						fprintf(stderr, "%s failed for stream %d, codec %s",  
							out_stream_info->m_vbsf_aac_adtstoasc->filter->name,audiopacket_t.stream_index,out_stream_info->m_oaudio_st->codec->codec ?  out_stream_info->m_oaudio_st->codec->codec->name : "copy");  
						av_free_packet(&audiopacket_t);     
 
					}  
				}  
			}  
		}  
		ret = av_interleaved_write_frame(out_stream_info->m_ocodec, &audiopacket_t);  
		if (ret != 0)  
		{  
			printf("error av_interleaved_write_frame _ audio\n");  
		}  
		printf("audio\n");  
	} 
}
 
int ffmpeg_transcode(int original_user_stream_id)
{
	int ret = 0;  
	int is_audio_decodefinish = 0;           //音频解码成功
	int is_video_decodefinish = 0;           //视频解码成功
	AVFrame * m_pin_temp_video_frame = av_frame_alloc();
	AVFrame * m_pin_temp_audio_frame = av_frame_alloc();
 
	if (m_list_out_stream_info.size()> 0)
	{
		map<int,Out_stream_info*> ::iterator result_all;
		Out_stream_info * out_stream_info_all = NULL;
		for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
		{
			out_stream_info_all = result_all->second;
			//如果有输出流
			if(out_stream_info_all)
			{  
				out_stream_info_all->m_pout_audio_frame = av_frame_alloc();
				out_stream_info_all->m_pout_video_frame = av_frame_alloc();
				out_stream_info_all->m_pout_video_frame->pts = 0;
				out_stream_info_all->m_pout_audio_frame->pts = 0;
				out_stream_info_all->m_pout_video_frame->width = out_stream_info_all->m_dwWidth;
				out_stream_info_all->m_pout_video_frame->height = out_stream_info_all->m_dwHeight;
				out_stream_info_all->m_pout_video_frame->format = out_stream_info_all->m_video_pixelfromat;
 
				int Out_size = avpicture_get_size((AVPixelFormat)out_stream_info_all->m_video_pixelfromat, out_stream_info_all->m_dwWidth,out_stream_info_all->m_dwHeight);  
				uint8_t * pOutput_buf =( uint8_t *)malloc(Out_size * 3 * sizeof(char)); //最大分配的空间，能满足yuv的各种格式  
				avpicture_fill((AVPicture *)out_stream_info_all->m_pout_video_frame, (unsigned char *)pOutput_buf, 
					(AVPixelFormat)out_stream_info_all->m_video_pixelfromat, out_stream_info_all->m_dwWidth,out_stream_info_all->m_dwHeight); //内存关联  
 
				av_frame_unref(out_stream_info_all->m_pout_audio_frame);  
			}
			result_all ++;
		}
	}
 
 
	//开始解包  
	while (1)  
	{  
		av_init_packet(&m_in_pkt);  
		if (av_read_frame(m_icodec, &m_in_pkt) < 0)  
		{  
			break;  
		}  
 
		//视频  
		if(m_in_pkt.stream_index == m_in_video_stream_idx)   
		{  
			if (m_list_out_stream_info.size()> 0)
			{
				map<int,Out_stream_info*> ::iterator result_all;
				Out_stream_info * out_stream_info_all = NULL;
				for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
				{
					out_stream_info_all = result_all->second;
					//如果有输出流
					if(out_stream_info_all)
					{  
						//原始流只需要copy的
						if(out_stream_info_all->user_stream_id == original_user_stream_id)
						{
							//如果纯copy支持拷贝的格式
							if (out_stream_info_all->m_writeheader_seccess ==1)
							{
								ffmpeg_write_frame(out_stream_info_all,OUT_VIDEO_ID,m_in_pkt); 
							}
							//在这里解码是因为输出的多路流中拷贝的只有一路，即在这里只解码一次，
							//其他需要重新编解码的用这个解码的m_pin_temp_video_frame的数据拷贝过去即可，在这里解码避免重复解码
							ret = ffmpeg_perform_decode(OUT_VIDEO_ID,m_pin_temp_video_frame); 
							if (ret == 0)  
							{
								is_video_decodefinish = 1;
							}
						}
						else if(out_stream_info_all->m_writeheader_seccess ==1)
						{ 
							if (is_video_decodefinish == 1)  
							{  
								ffmpeg_perform_yuv_conversion(out_stream_info_all,m_pin_temp_video_frame,out_stream_info_all->m_pout_video_frame);  
								ret = ffmpeg_perform_code2(out_stream_info_all,OUT_VIDEO_ID,out_stream_info_all->m_pout_video_frame);
							}  
						}
						else
						{
 
						}
					}
					result_all ++;
				}
				is_video_decodefinish = 0;
			}
		}  
		//音频  
		else if (m_in_pkt.stream_index == m_in_audio_stream_idx)  
		{  
			if (m_list_out_stream_info.size()> 0)
			{
				map<int,Out_stream_info*> ::iterator result_all;
				Out_stream_info * out_stream_info_all = NULL;
				for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
				{
					out_stream_info_all = result_all->second;
					//如果有输出流
					if(out_stream_info_all)
					{   
						//原始流只需要copy的
						if(out_stream_info_all->user_stream_id == original_user_stream_id)
						{
							//如果纯copy支持拷贝的格式
							if (out_stream_info_all->m_writeheader_seccess ==1)
							{
								ffmpeg_write_frame(out_stream_info_all,OUT_AUDIO_ID,m_in_pkt);  
							}
							//在这里解码是因为输出的多路流中拷贝的只有一路，即在这里只解码一次，
							//其他需要重新编解码的用这个解码的m_pin_temp_audio_frame的数据拷贝过去即可，在这里解码避免重复解码
							ret = ffmpeg_perform_decode(OUT_AUDIO_ID,m_pin_temp_audio_frame); 
							if (ret == 0)  
							{
								is_audio_decodefinish = 1;
							}
						}
						else if(out_stream_info_all->m_writeheader_seccess ==1)
						{
							if (is_audio_decodefinish == 1)  
							{  
								if (out_stream_info_all->m_swr_ctx == NULL)  
								{  
									out_stream_info_all->m_swr_ctx = ffmpeg_init_pcm_resample(out_stream_info_all,m_pin_temp_audio_frame,out_stream_info_all->m_pout_audio_frame);  
								}  
								ffmpeg_preform_pcm_resample(out_stream_info_all,out_stream_info_all->m_swr_ctx,m_pin_temp_audio_frame,out_stream_info_all->m_pout_audio_frame);    
								ffmpeg_perform_code2(out_stream_info_all,OUT_AUDIO_ID,out_stream_info_all->m_pout_audio_frame); 
							} 
						}
						else
						{
 
						}
					}
					result_all ++;
				}
				is_audio_decodefinish = 0;
			}
		} 
	}   
 
	if (m_list_out_stream_info.size()> 0)
	{
		map<int,Out_stream_info*> ::iterator result_all;
		Out_stream_info * out_stream_info_all = NULL;
		for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
		{
			out_stream_info_all = result_all->second;
			//如果有输出流
			if(out_stream_info_all)
			{    
				if (out_stream_info_all->m_pout_audio_frame)
				{
					av_frame_free(&out_stream_info_all->m_pout_audio_frame);
					out_stream_info_all->m_pout_audio_frame = NULL;
				}
				if (out_stream_info_all->m_pout_video_frame)
				{
					av_frame_free(&out_stream_info_all->m_pout_video_frame);
					out_stream_info_all->m_pout_video_frame = NULL;
				}
 
				ffmpeg_uinit_pcm_resample(out_stream_info_all->m_swr_ctx,out_stream_info_all->m_audiofifo);
			}
			result_all ++;
		}
	}
 
	if (m_pin_temp_video_frame)
	{
		av_frame_free(&m_pin_temp_video_frame);
		m_pin_temp_video_frame = NULL;
	}
	if (m_pin_temp_audio_frame)
	{
		av_frame_free(&m_pin_temp_audio_frame);
		m_pin_temp_audio_frame = NULL;
	}
 
	ret = 1;
	return ret;
}


