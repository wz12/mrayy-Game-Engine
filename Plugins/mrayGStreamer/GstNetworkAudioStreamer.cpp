

#include "stdafx.h"
#include "GstNetworkAudioStreamer.h"

#include "GstPipelineHandler.h"

#include "CMyUDPSrc.h"
#include "CMyUDPSink.h"





namespace mray
{
namespace video
{



class GstNetworkAudioStreamerImpl :public GstPipelineHandler
{
protected:
	core::string m_ipAddr;
	int m_audioPort;

	core::string m_pipeLineString;
	GstMyUDPSink* m_audioSink;
	GstMyUDPSink* m_audioRtcpSink;
	GstMyUDPSrc* m_audioRtcpSrc;
public:
	GstNetworkAudioStreamerImpl()
	{
	}
	virtual ~GstNetworkAudioStreamerImpl()
	{
		m_ipAddr = "127.0.0.1";
		m_audioPort = 5005;
	}

	void BuildString()
	{
		core::string audioStr="directsoundsrc! audio/x-raw,endianness=1234,signed=true,width=16,depth=16,rate=8000,channels=1   ! amrnbenc ! rtpamrpay";
		m_pipeLineString = audioStr + " ! "
			"myudpsink name=audioSink ";


	}
	void _UpdatePorts()
	{

		if (!m_gstPipeline)
			return;
#define SET_SRC(name,p) m_##name=GST_MyUDPSrc(gst_bin_get_by_name(GST_BIN(m_gstPipeline), #name)); if(m_##name){m_##name->SetPort(p);}
#define SET_SINK(name,p) m_##name=GST_MyUDPSink(gst_bin_get_by_name(GST_BIN(m_gstPipeline), #name)); if(m_##name){m_##name->SetPort(m_ipAddr,p);}


		SET_SINK(audioSink, m_audioPort);
		SET_SRC(audioRtcpSrc, (m_audioPort + 1));
		SET_SINK(audioRtcpSink, (m_audioPort + 2));

	}

	// addr: target address to stream video to
	// audioport: port for the audio stream , audio rtcp is allocated as audioPort+1 and audioPort+2
	void BindPorts(const core::string& addr, int audioPort)
	{
		m_ipAddr = addr;
		m_audioPort = audioPort;
		_UpdatePorts();
	}

	bool CreateStream()
	{
		GError *err = 0;
		BuildString();
		m_gstPipeline = gst_parse_launch(m_pipeLineString.c_str(), &err);
		if (err)
		{
			printf("GstAudioNetworkStreamer: Pipeline error: %s", err->message);
		}
		if (!m_gstPipeline)
			return false;
		_UpdatePorts();

		return CreatePipeline();

	}
	void Stream()
	{
		SetPaused(false);
	}
	bool IsStreaming()
	{
		return !m_paused;
	}
	virtual void Close()
	{
		GstPipelineHandler::Close();
	}

};


GstNetworkAudioStreamer::GstNetworkAudioStreamer()
{
	m_impl = new GstNetworkAudioStreamerImpl();
}

GstNetworkAudioStreamer::~GstNetworkAudioStreamer()
{
	delete m_impl;
}
void GstNetworkAudioStreamer::Stream()
{
	m_impl->Stream();
}
void GstNetworkAudioStreamer::Stop()
{
	m_impl->Stop();
}


void GstNetworkAudioStreamer::BindPorts(const core::string& addr,  int audioPort)
{
	m_impl->BindPorts(addr,  audioPort);
}

bool GstNetworkAudioStreamer::CreateStream()
{
	return m_impl->CreateStream();
}

void GstNetworkAudioStreamer::Close()
{
	m_impl->Close();
}
bool GstNetworkAudioStreamer::IsStreaming()
{
	return m_impl->IsStreaming();
}

}
}