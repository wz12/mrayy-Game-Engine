

#ifndef GstPipelineHandler_h__
#define GstPipelineHandler_h__

#include "mtypes.h"
#include "mstring.h"
#include "ListenerContainer.h"
#include <gst/gst.h>


namespace mray
{
namespace video
{

	class GstPipelineHandlerImpl;
	class GstPipelineHandler;

	class IPipelineListener
	{
	public:
		virtual void OnPipelineReady(GstPipelineHandler* p){}
		virtual void OnPipelinePlaying(GstPipelineHandler* p){}
		virtual void OnPipelineStopped(GstPipelineHandler* p){}
		virtual void OnPipelineError(GstPipelineHandler* p){}
	};
	
class GstPipelineHandler :public ListenerContainer<IPipelineListener*>
{
protected:

	GstPipelineHandlerImpl* m_data;

	DECLARE_FIRE_METHOD(OnPipelineReady, (GstPipelineHandler* p), (p));
	DECLARE_FIRE_METHOD(OnPipelinePlaying, (GstPipelineHandler* p), (p));
	DECLARE_FIRE_METHOD(OnPipelineStopped, (GstPipelineHandler* p), (p));
	DECLARE_FIRE_METHOD(OnPipelineError, (GstPipelineHandler* p), (p));
public:
	GstPipelineHandler();
	virtual ~GstPipelineHandler();

	//set isMasterClock to true if 
	virtual bool CreatePipeline();
	virtual void SetPaused(bool p);
	virtual void Stop();
	virtual bool IsLoaded();
	virtual bool IsPlaying();
	virtual bool IsPaused();
	virtual void Close();
	virtual bool HandleMessage(GstBus * bus, GstMessage * msg);

	virtual bool QueryLatency(bool &isLive,ulong& minLatency,ulong& maxLatency);

	virtual void SetClockBaseTime(ulong baseTime);
	virtual ulong GetClockBaseTime();

	void SetClockAddr(const core::string& host, int port);
	int GetClockPort();

	void SetPipeline(GstElement* p);
	GstElement* GetPipeline();

	static bool busFunction(GstBus * bus, GstMessage * message, GstPipelineHandler * player);


	
};

}
}

#endif // GstPipelineHandler_h__
