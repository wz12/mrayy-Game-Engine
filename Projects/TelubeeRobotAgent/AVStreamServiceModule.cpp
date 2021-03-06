

#include "stdafx.h"
#include "AVStreamServiceModule.h"
#include "TBeeServiceContext.h"
#include "GstStreamBin.h"
#include "GstVideoProvider.h"
#include "ICameraVideoGrabber.h"
#include "VideoGrabberTexture.h"
#include "IThreadManager.h"
#include "CameraProfile.h"
#include "GstNetworkVideoStreamer.h"
#include "GstNetworkAudioStreamer.h"
#include "FlyCameraVideoGrabber.h"
#include "DirectShowVideoGrabber.h"
#include "GstCustomVideoStreamer.h"
#include "GstCustomMultipleVideoStreamer.h"
#include "FlyCameraManager.h"
#include "DirectSoundInputStream.h"
#include "CommunicationMessages.h"
#include "StreamReader.h"
#include "XMLTree.h"

namespace mray
{
namespace TBee
{

	IMPLEMENT_RTTI(AVStreamServiceModule, IServiceModule);

	const std::string AVStreamServiceModule::ModuleName("AVStreamServiceModule");


	class GstVideoGrabberImpl :public GstVideoGrabber
	{
		OS::IMutex* m_mutex;
		video::IVideoGrabber* m_grabber;
	public:
		GstVideoGrabberImpl(video::IVideoGrabber* v)
		{
			m_grabber = v;
			m_mutex = OS::IThreadManager::getInstance().createMutex();
		}
		~GstVideoGrabberImpl()
		{
			delete m_mutex;
		}
		virtual void Lock()
		{
			m_mutex->lock();
		}
		virtual void Unlock()
		{
			m_mutex->unlock();
		}
		virtual video::IVideoGrabber* GetGrabber()
		{
			return m_grabber;
		}
	};
class AVStreamServiceModuleImpl:public video::IGStreamerStreamerListener,public IServiceContextListener
{
public:


	enum class ECameraType
	{
		Webcam,
		PointGrey
	};
	GCPtr<video::GstStreamBin> m_streamers;
	ECameraType m_cameraType;
	math::vector2di m_resolution;
	int m_fps;


	video::VideoGrabberTexture m_cameraTextures[2];
	float m_exposureValue;
	float m_gainValue;
	float m_gammaValue;
	float m_WBValue;

	bool m_streamAudio;

	struct CameraSettings
	{
		CameraSettings(){}
		CameraSettings(const math::vector2d& sz, int bits, int f)
		{
			size = sz;
			bitrate = bits;
			fps = f;
		}
		math::vector2di size;
		int bitrate;
		int fps;
	};
	std::vector<CameraSettings> m_cameraSettings;

	struct _CameraInfo
	{
		CameraInfo ifo;
		int w, h, fps;
		GCPtr<video::ICameraVideoGrabber> camera;
		math::vector2di offsets;
	}m_cameraIfo[2];

	int m_quality;
	CameraProfileManager* m_cameraProfileManager;
	core::string m_cameraProfile;

	EServiceStatus m_status;

	TBeeServiceContext* m_context;

public:
	AVStreamServiceModuleImpl()
	{
		m_status = EServiceStatus::Idle;
		m_cameraProfileManager = new CameraProfileManager();

		m_streamers = new video::GstStreamBin();
		m_exposureValue = -1;
		m_gainValue = 0.1;
		m_WBValue = -1;
		m_gammaValue = 0.0;
		LoadCameraSettings("StreamingProfiles.xml");
	}
	~AVStreamServiceModuleImpl()
	{
		Destroy();
		m_streamers = 0;
		delete m_cameraProfileManager;
	}

	void LoadCameraSettings(const core::string &src)
	{
		xml::XMLTree tree;
		core::string path;
		gFileSystem.getCorrectFilePath(src, path);

		if (path=="" || tree.load(path) == false)
		{
			m_cameraSettings.push_back(CameraSettings(math::vector2di(640, 480), 3000, 30));
			m_cameraSettings.push_back(CameraSettings(math::vector2di(640, 480), 4000, 45));
			m_cameraSettings.push_back(CameraSettings(math::vector2di(640, 480), 5000, 50));
			m_cameraSettings.push_back(CameraSettings(math::vector2di(960, 720), 6000, 45));
			m_cameraSettings.push_back(CameraSettings(math::vector2di(960,720), 7000, 50));
			return;
		}

		xml::XMLElement* e= tree.getSubElement("Settings");
		e = e->getSubElement("Setting");

		while (e)
		{
			CameraSettings s;

			s.size = core::StringConverter::toVector2d( e->getValueString("Resolution"));
			s.fps = e->getValueInt("FPS");
			s.bitrate = e->getValueInt("Bitrate");
			m_cameraSettings.push_back(s);
			e = e->nextSiblingElement("Setting");
		}
	}

	void SetCameraParameterValue(const core::string& name, const core::string& value)
	{

		for (int i = 0; i < 2; ++i)
		{
			if (m_cameraIfo[i].camera)
			{
				m_cameraIfo[i].camera->SetParameter(name, value);

				printf("Camera [%d] %s value is set to: %s\n", i, name.c_str(), m_cameraIfo[i].camera->GetParameter(name).c_str());
			}
		}
	}

	void Init(TBeeServiceContext* context)
	{
		if (m_status != EServiceStatus::Idle)
			return;

		m_context = context;

		m_cameraIfo[0].w = m_cameraIfo[1].w = 1280;
		m_cameraIfo[0].h = m_cameraIfo[1].h = 720;
		m_cameraIfo[0].fps = m_cameraIfo[1].fps = 45;
		m_resolution.set(1280, 720);

#if USE_POINTGREY && USE_WEBCAMERA
		m_cameraType = context->appOptions.GetOptionByName("CameraConnection")->getValue() == "DirectShow" ? ECameraType::Webcam : ECameraType::PointGrey;
#else 
#if USE_POINTGREY
		m_cameraType=ECameraType::PointGrey;
#else
		m_cameraType = ECameraType::Webcam;
#endif

#endif
		m_cameraProfile = context->appOptions.GetOptionValue("CameraProfile");
		m_quality = core::StringConverter::toInt(context->appOptions.GetOptionByName("Quality")->getValue());
		core::string res = context->appOptions.GetOptionByName("StreamResolution")->getValue();

		if (res == "0-VGA")
			m_resolution.set(640, 480);
		else if (res == "1-HD")
			m_resolution.set(1280, 720);
		else if (res == "2-FullHD")
			m_resolution.set(1920, 1080);
		m_streamAudio = context->appOptions.GetOptionByName("Audio")->getValue() == "Yes";


		if (m_cameraType == ECameraType::Webcam)
		{
			// -1 for the None index
			m_cameraIfo[0].ifo.index = context->appOptions.GetOptionByName("DS_Camera_Left")->getValueIndex() - 1;
			m_cameraIfo[1].ifo.index = context->appOptions.GetOptionByName("DS_Camera_Right")->getValueIndex() - 1;
		}
		else
		{
			//point grey cameras have unique serial number
			int count = video::FlyCameraManager::getInstance().GetCamerasCount();
			int c1 = core::StringConverter::toInt(context->appOptions.GetOptionByName("PT_Camera_Left")->getValue());
			int c2 = core::StringConverter::toInt(context->appOptions.GetOptionByName("PT_Camera_Right")->getValue());
			for (int i = 0; i < count; ++i)
			{
				uint sp;
				video::FlyCameraManager::getInstance().GetCameraSerialNumber(i, sp);
				if (sp == c1)
				{
					m_cameraIfo[0].ifo.index = i;
				}
				if (sp == c2)
				{
					m_cameraIfo[1].ifo.index = i;
				}

			}
		}



		{
			std::vector<sound::InputStreamDeviceInfo> lst;
			sound::DirectSoundInputStream inputStream;
			inputStream.ListDevices(lst);
			for (int i = 0; i < lst.size(); ++i)
			{
				printf("%d - %s : %s\n", lst[i].ID, lst[i].name.c_str(), lst[i].description.c_str());
			}
		}

		{
			//load camera cropping offsets

			OS::IStreamPtr s = gFileSystem.openFile(gFileSystem.getAppPath() + "CameraOffsets.txt", OS::TXT_READ);
			if (!s.isNull())
			{
				OS::StreamReader rdr(s);
				core::StringConverter::parse(rdr.readLine(), m_cameraIfo[0].offsets);
				core::StringConverter::parse(rdr.readLine(), m_cameraIfo[1].offsets);
				s->close();
			}
		}
		printf("Initializing Cameras\n");
		m_quality = math::Min<int>((int)m_quality, m_cameraSettings.size());
		CameraSettings setting = m_cameraSettings[m_quality];
		math::vector2d capRes = m_resolution = setting.size;
		int fps = m_fps = setting.fps;

		for (int i = 0; i < 2; ++i)
		{
			m_cameraIfo[i].fps = fps;
			m_cameraIfo[i].w = capRes.x;
			m_cameraIfo[i].h = capRes.y;
		}
#if USE_WEBCAMERA
		if (m_cameraType == ECameraType::Webcam)
		{		
			video::GstNetworkVideoStreamer* vstreamer;

			for (int i = 0; i < 2; ++i)
			{
				if (m_cameraIfo[i].ifo.index >= 0)
				{
					m_cameraIfo[i].camera = new video::DirectShowVideoGrabber();
					m_cameraIfo[i].camera->InitDevice(m_cameraIfo[i].ifo.index, m_cameraIfo[i].w, m_cameraIfo[i].h, m_cameraIfo[i].fps);//1280, 720
					m_cameraIfo[i].ifo.guidPath = m_cameraIfo[i].camera->GetDeviceName(m_cameraIfo[i].ifo.index);

				}
			}

			SetCameraParameterValue(video::ICameraVideoGrabber::Param_Focus, "0");
			SetCameraParameterValue(video::ICameraVideoGrabber::Param_Exposure, (m_exposureValue > 0 ? core::StringConverter::toString(m_exposureValue) : "auto"));
			SetCameraParameterValue(video::ICameraVideoGrabber::Param_Gain, (m_gainValue > 0 ? core::StringConverter::toString(m_gainValue) : "auto"));
			SetCameraParameterValue(video::ICameraVideoGrabber::Param_WhiteBalance, (m_WBValue > 0 ? core::StringConverter::toString(m_WBValue) : "auto"));
			SetCameraParameterValue(video::ICameraVideoGrabber::Param_Gamma, (m_gammaValue > 0 ? core::StringConverter::toString(m_gammaValue) : "auto"));

			// Now close cameras
			for (int i = 0; i < 2; ++i)
			{
				if (m_cameraIfo[i].camera)
					m_cameraIfo[i].camera->Stop();
			}

			if (true)
			{
				vstreamer = new video::GstNetworkVideoStreamer();
				vstreamer->AddListener(this);

				vstreamer->SetCameraResolution(m_cameraIfo[0].w, m_cameraIfo[0].h, fps);
				vstreamer->SetFrameResolution(m_resolution.x, m_resolution.y);
				vstreamer->SetCameras(m_cameraIfo[0].ifo.index, m_cameraIfo[1].ifo.index);
				vstreamer->SetBitRate(bitRate[(int)m_quality]);


				m_streamers->AddStream(vstreamer, "Video");

			}
			else
			{
				// 				m_cameraIfo[0].camera->Start();
				// 				m_cameraIfo[1].camera->Start();


				video::GstCustomVideoStreamer* hs = new video::GstCustomVideoStreamer();

				hs->AddListener(this);
				hs->SetVideoGrabber(m_cameraIfo[0].camera, m_cameraIfo[1].camera);//
				hs->SetBitRate(bitRate[(int)m_quality]);
				hs->SetResolution(m_resolution.x, m_resolution.y, fps);
				m_streamers->AddStream(hs, "Video");
			}

		}
#endif
#if USE_POINTGREY && USE_WEBCAMERA
		else
#endif
#if USE_POINTGREY
		{
			if (m_cameraIfo[0].ifo.index != -1)
			{
				printf("Initializing Pointgrey Camera\n");
				video::FlyCameraVideoGrabber* c = 0;
				m_cameraIfo[0].camera = (c=new video::FlyCameraVideoGrabber());
				c->SetCroppingOffset(m_cameraIfo[0].offsets.x, m_cameraIfo[0].offsets.y);
				m_cameraIfo[0].camera->InitDevice(m_cameraIfo[0].ifo.index, m_cameraIfo[0].w, m_cameraIfo[0].h, fps);
				m_cameraIfo[0].camera->SetImageFormat(video::EPixel_R8G8B8);
				//	m_cameraIfo[0].camera->Start();
			}

			if (m_cameraIfo[1].ifo.index != -1
				&& m_cameraIfo[0].ifo.index != m_cameraIfo[1].ifo.index)
			{
				printf("Initializing Pointgrey Camera\n");
				video::FlyCameraVideoGrabber* c = 0;
				m_cameraIfo[1].camera = (c = new video::FlyCameraVideoGrabber());
				c->SetCroppingOffset(m_cameraIfo[1].offsets.x, m_cameraIfo[1].offsets.y);
				m_cameraIfo[1].camera->InitDevice(m_cameraIfo[1].ifo.index, m_cameraIfo[1].w, m_cameraIfo[1].h, fps);
				m_cameraIfo[1].camera->SetImageFormat(video::EPixel_R8G8B8);
				//	m_cameraIfo[1].camera->Start();
			}

			//Bug: Open and close the cameras once, needed to make the camera run next time
			for (int i = 0; i < 2; ++i)
			{
				if (m_cameraIfo[i].camera)
				{
					m_cameraIfo[i].camera->Start();
					m_cameraIfo[i].camera->Stop();
				}
			}

			printf("Creating Video Streamer\n");
			video::GstCustomMultipleVideoStreamer* hs = new video::GstCustomMultipleVideoStreamer();
			hs->AddListener(this);

			std::vector<video::IVideoGrabber*> grabbers;
			grabbers.push_back(m_cameraIfo[0].camera);
			grabbers.push_back(m_cameraIfo[1].camera);
			hs->SetVideoGrabber(grabbers);//
			hs->SetResolution(m_resolution.x, m_resolution.y, fps, true);
			hs->SetBitRate(setting.bitrate);
			m_streamers->AddStream(hs, "Video");
		}
#endif

		//Create Audio Stream
		if (m_streamAudio)
		{
			video::GstNetworkAudioStreamer* streamer;
			streamer = new video::GstNetworkAudioStreamer();
			m_streamers->AddStream(streamer, "Audio");
		}

		//setup cameras settings
		core::string camParams[] = {
			video::ICameraVideoGrabber::Param_Brightness,
			video::ICameraVideoGrabber::Param_Contrast,
			video::ICameraVideoGrabber::Param_Hue,
			video::ICameraVideoGrabber::Param_Saturation,
			video::ICameraVideoGrabber::Param_Sharpness,
			video::ICameraVideoGrabber::Param_ColorEnable,
			video::ICameraVideoGrabber::Param_WhiteBalance,
			video::ICameraVideoGrabber::Param_Gain,
			video::ICameraVideoGrabber::Param_BacklightCompensation,
			video::ICameraVideoGrabber::Param_Exposure,
			video::ICameraVideoGrabber::Param_Iris,
			video::ICameraVideoGrabber::Param_Focus
		};
		int count = sizeof(camParams) / sizeof(core::string);
		//init cameras
		CameraProfile* prof = m_cameraProfileManager->GetProfile(m_cameraProfile);
		{
			for (int i = 0; i < 2; ++i)
			{

				if (prof && false)
				{
					for (int j = 0; j < count; ++j)
					{
						core::string v;
						if (prof->GetValue(camParams[j], v))
							m_cameraIfo[i].camera->SetParameter(camParams[j], v);
					}
				}
				//	if (!m_debugData.debug)
				if (m_cameraType == ECameraType::Webcam)
				{
					//	m_cameraIfo[i].camera->Stop();
				}
				m_cameraTextures[i].Set(m_cameraIfo[i].camera, gEngine.getDevice()->createEmptyTexture2D(true));

			}
		}

		context->AddListener(this);
		m_status = EServiceStatus::Inited;
	}
	void Destroy()
	{
		if (m_status == EServiceStatus::Idle)
			return;
		StopStream();

		m_context->RemoveListener(this);
		m_streamers->ClearStreams(true);
		m_cameraTextures[0].Set(0, 0);
		m_cameraTextures[1].Set(0, 0);
		if (m_cameraIfo[0].camera)
		{
			m_cameraIfo[0].camera->Stop();
			m_cameraIfo[0].camera = 0;
		}
		if (m_cameraIfo[1].camera)
		{
			m_cameraIfo[1].camera->Stop();
			m_cameraIfo[1].camera = 0;
		}

		m_status = EServiceStatus::Idle;
	}


	void StartStream()
	{
		if (m_status != EServiceStatus::Inited && m_status!=EServiceStatus::Stopped)
			return;

		printf("Starting Stream at :%dx%d@%d\n", m_resolution.x, m_resolution.y, m_fps);
		//  Begin the video stream
		if (m_cameraType == ECameraType::PointGrey)
		{
			//Pointgrey cameras need to be started manually
			printf("Starting Cameras.\n");
			if (m_cameraIfo[0].camera)
				m_cameraIfo[0].camera->Start();

			if (m_cameraIfo[1].camera)
				m_cameraIfo[1].camera->Start();
		}

		m_streamers->GetStream("Video")->CreateStream();
		if (m_streamAudio)
			m_streamers->GetStream("Audio")->CreateStream();
		m_streamers->Stream();
		m_status = EServiceStatus::Running;
	}

	bool StopStream()
	{
		if (m_status != EServiceStatus::Running)
			return false;
		printf("Stopping AVStreamService.\n");

		m_streamers->CloseAll();

		printf("Stopping Cameras.\n");
		m_cameraIfo[0].camera->Stop();
		m_cameraIfo[1].camera->Stop();

		m_status = EServiceStatus::Stopped;
		return true;
	}


	void Update()
	{

		if (m_status != EServiceStatus::Running)
			return;

		//check camera condition if connected or not
		if (m_cameraType == ECameraType::PointGrey)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (m_cameraIfo[i].camera && m_cameraIfo[i].camera->IsConnected() == false)
				{
				//	printf("Camera %d has stopped, restarting it\n", i);
					m_cameraIfo[i].camera->Start();

					//give the camera sometime to start
					OS::IThreadManager::getInstance().sleep(30);
				}
			}
		}
	}


	void DebugRender(TbeeServiceRenderContext* context)
	{

		core::string msg = "[" + AVStreamServiceModule::ModuleName + "] Service Status: " + IServiceModule::ServiceStatusToString(m_status);

		if (m_status == EServiceStatus::Idle)
			return;

		if (m_cameraIfo[0].camera)
		{

			 msg = "Capture FPS: " + core::StringConverter::toString(m_cameraIfo[0].camera->GetCaptureFrameRate());
			context->RenderText(msg, 100, 0);
		}
		if (m_cameraType == ECameraType::PointGrey)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (m_cameraIfo[i].camera && m_cameraIfo[i].camera->IsConnected() == false)
				{
					msg = "Camera: " + core::StringConverter::toString(i)+" is not open!";
					context->RenderText(msg, 100, 0,video::SColor(1,0,0,1));
				}
			}
		}
	}
	void Render(TbeeServiceRenderContext* context)
	{
		if (m_status == EServiceStatus::Idle)
			return;

	}


	CameraProfileManager* LoadCameraProfiles(const core::string& path)
	{
		m_cameraProfileManager->LoadFromXML(path);
		return m_cameraProfileManager;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Listeners
	virtual void OnUserConnected(const UserConnectionData& data)
	{
		const int BufferLen = 128;
		uchar buffer[BufferLen];
		//tell the client if we are sending stereo or single video images
		OS::CMemoryStream stream("", buffer, BufferLen, false, OS::BIN_WRITE);
		OS::StreamWriter wrtr(&stream);

		if (m_streamers)
		{
			m_streamers->GetStream("Video")->BindPorts(data.address.toString(), data.videoPort, data.clockPort, data.rtcp);
			if (m_streamAudio)
				m_streamers->GetStream("Audio")->BindPorts(data.address.toString(), data.audioPort, data.clockPort + 1, data.rtcp);
		

			int reply = (int)EMessages::IsStereo;
			int len = stream.write(&reply, sizeof(reply));
			bool stereo = m_cameraIfo[0].ifo.index != m_cameraIfo[1].ifo.index &&
				m_cameraIfo[0].ifo.index != -1 && m_cameraIfo[1].ifo.index != -1;// ((video::GstNetworkVideoStreamer*)m_streamers->GetStream("Video"))->IsStereo();
			len += stream.write(&stereo, sizeof(stereo));
			m_context->commChannel->SendTo(&m_context->remoteAddr, (char*)buffer, len);

		}

		if (false)
		{
			stream.seek(0, OS::ESeek_Set);
			int reply = (int)EMessages::ClockSync;
			int len = stream.write(&reply, sizeof(reply));

			ulong baseClock = m_streamers->GetStream("Video")->GetClockBase();

			len += wrtr.binWriteInt(baseClock);
			m_context->commChannel->SendTo(&m_context->remoteAddr, (char*)buffer, len);
		}

		{
			stream.seek(0, OS::ESeek_Set);
			int reply = (int)EMessages::CameraConfig;
			int len = stream.write(&reply, sizeof(reply));
			len += wrtr.binWriteString(m_cameraProfile);
			m_context->commChannel->SendTo(&m_context->remoteAddr, (char*)buffer, len);
		}
	}

};

AVStreamServiceModule::AVStreamServiceModule()
{
	m_impl = new AVStreamServiceModuleImpl();
}

AVStreamServiceModule::~AVStreamServiceModule()
{
	delete m_impl;
}


std::string AVStreamServiceModule::GetServiceName()
{
	return AVStreamServiceModule::ModuleName;
}

EServiceStatus AVStreamServiceModule::GetServiceStatus()
{
	return m_impl->m_status;
}

void AVStreamServiceModule::InitService(ServiceContext* context)
{
	m_impl->Init((TBeeServiceContext*)context);
}

EServiceStatus AVStreamServiceModule::StartService(ServiceContext* context)
{
	m_impl->StartStream();
	return m_impl->m_status;
}

bool AVStreamServiceModule::StopService()
{
	return m_impl->StopStream();
}

void AVStreamServiceModule::DestroyService()
{
	m_impl->Destroy();
}


void AVStreamServiceModule::Update(float dt)
{
	m_impl->Update();
}

void AVStreamServiceModule::Render(ServiceRenderContext* contex)
{
	m_impl->Render((TbeeServiceRenderContext*)contex);
}

void AVStreamServiceModule::DebugRender(ServiceRenderContext* contex)
{
	m_impl->DebugRender((TbeeServiceRenderContext*)contex);
}

bool AVStreamServiceModule::LoadServiceSettings(xml::XMLElement* e)
{
	return true;
}

void AVStreamServiceModule::ExportServiceSettings(xml::XMLElement* e)
{
}

}
}

