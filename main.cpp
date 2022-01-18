#include "ffmpeg_transcode.h"
 
int main(int argc ,char ** argv)
{
	int ret = 0;
 
	av_register_all();
	avformat_network_init();
 
	ffmpeg_init_demux(INPUTURL,&m_icodec);
 
	//out_stream1 用原始的
	Out_stream_info *out_stream_info1 = NULL;
	out_stream_info1 = new Out_stream_info();
	out_stream_info1->user_stream_id = 10;
	sprintf(out_stream_info1->m_outurlname,"%s",OUTPUTURL10);
	//out_stream2
	Out_stream_info *out_stream_info2 = NULL;
	out_stream_info2 = new Out_stream_info();
	out_stream_info2->user_stream_id = 11;
	sprintf(out_stream_info2->m_outurlname,"%s",OUTPUTURL11);
	out_stream_info2->m_dwWidth = 640;
	out_stream_info2->m_dwHeight = 480;
	out_stream_info2->m_dbFrameRate = 25;
	out_stream_info2->m_video_codecID = (int)AV_CODEC_ID_H264;
	out_stream_info2->m_video_pixelfromat = (int)AV_PIX_FMT_YUV420P;
	out_stream_info2->m_bit_rate = 800000;
	out_stream_info2->m_gop_size = 125;
	out_stream_info2->m_max_b_frame = 0;
	out_stream_info2->m_thread_count = 8;
	out_stream_info2->m_dwChannelCount = 2;
	out_stream_info2->m_dwBitsPerSample = AV_SAMPLE_FMT_S16_t;
	out_stream_info2->m_dwFrequency = 48000;
	out_stream_info2->m_audio_codecID = (int)AV_CODEC_ID_AAC; 
	//out_stream3
	Out_stream_info *out_stream_info3 = NULL;
	out_stream_info3 = new Out_stream_info();
	out_stream_info3->user_stream_id = 12;
	sprintf(out_stream_info3->m_outurlname,"%s",OUTPUTURL12);
	out_stream_info3->m_dwWidth = 352;
	out_stream_info3->m_dwHeight = 288;
	out_stream_info3->m_dbFrameRate = 25;
	out_stream_info3->m_video_codecID = (int)AV_CODEC_ID_H264;
	out_stream_info3->m_video_pixelfromat = (int)AV_PIX_FMT_YUV420P;
	out_stream_info3->m_bit_rate = 400000;
	out_stream_info3->m_gop_size = 125;
	out_stream_info3->m_max_b_frame = 0;
	out_stream_info3->m_thread_count = 8;
	out_stream_info3->m_dwChannelCount = 2;
	out_stream_info3->m_dwBitsPerSample = AV_SAMPLE_FMT_S16_t;
	out_stream_info3->m_dwFrequency = 48000;
	out_stream_info3->m_audio_codecID = (int)AV_CODEC_ID_AAC; 
 
	//申请map
	m_list_out_stream_info[out_stream_info1->user_stream_id] = (out_stream_info1);
	m_list_out_stream_info[out_stream_info2->user_stream_id] = (out_stream_info2);
	m_list_out_stream_info[out_stream_info3->user_stream_id] = (out_stream_info3);
 
	ffmpeg_init_mux(m_list_out_stream_info,out_stream_info1->user_stream_id);
	printf("--------程序运行开始----------\n");
	//
	ffmpeg_transcode(out_stream_info1->user_stream_id);
	//
	ffmpeg_uinit_mux(m_list_out_stream_info,out_stream_info1->user_stream_id);
	ffmpeg_uinit_demux(m_icodec);
 
	//释放map
	if (m_list_out_stream_info.size()> 0)
	{
		map<int,Out_stream_info*> ::iterator result_all;
		Out_stream_info * out_stream_info_all = NULL;
		for (result_all = m_list_out_stream_info.begin();result_all != m_list_out_stream_info.end();)
		{
			out_stream_info_all = result_all->second;
			if(out_stream_info_all)
			{
				delete out_stream_info_all;
				out_stream_info_all = NULL;          
			}
			m_list_out_stream_info.erase(result_all ++);
		}
		m_list_out_stream_info.clear();
	}
	printf("--------程序运行结束----------\n");
	printf("-------请按任意键退出---------\n");
	return getchar();
}

