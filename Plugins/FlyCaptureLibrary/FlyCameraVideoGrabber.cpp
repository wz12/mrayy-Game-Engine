

#include "stdafx.h"
#include "FlyCameraVideoGrabber.h"
#include "FlyCameraManager.h"
#include "StringConverter.h"
#include "IThreadManager.h"
#include "IMutex.h"
#include "ColorConverter.h"
#include "ITimer.h"
#include "Engine.h"

using namespace FlyCapture2;


namespace mray
{
namespace video
{

	struct CMode
	{
		VideoMode m;
		int width,height;

		int getDistance(int w,int h)const
		{
			return abs(w-width)+abs(h-height);
		}


		static FlyCapture2::PixelFormat GetFormat(EPixelFormat fmt)
		{
			switch (fmt)
			{
			case mray::video::EPixel_LUMINANCE8:
			case mray::video::EPixel_Alpha8:
				return PIXEL_FORMAT_MONO8;
			case mray::video::EPixel_LUMINANCE16:
				return PIXEL_FORMAT_MONO16;

			default:
				return PIXEL_FORMAT_422YUV8;
				break;
			}
		}

	};
	const CMode g_modes[]=
	{
		VIDEOMODE_320x240YUV422,320,240,
		VIDEOMODE_640x480YUV422, 640, 480,
		VIDEOMODE_800x600YUV422, 800, 600,
		VIDEOMODE_1024x768YUV422, 1024, 768,
		VIDEOMODE_1280x960YUV422, 1280, 960,
		VIDEOMODE_1600x1200YUV422, 1600, 1200,
	};

	CMode GetClosestVideoMode(int w,int h)
	{
		int closest=0;;
		int dist=9999999;
		for(int i=1;i<6;++i)
		{
			int d=g_modes[i].getDistance(w,h);
			if(d<dist)
			{
				closest=i;
				dist=d;
			}
		}
		return g_modes[closest];
	}
	const CMode *GetClosestVideoMode(VideoMode mode)
	{
		for(int i=0;i<6;++i)
		{
			if(g_modes[i].m==mode)
				return &g_modes[i];
		}
		return 0;
	}


	struct CFpsMode
	{
		FrameRate f;
		int fps;
	};
	const CFpsMode g_fps[]=
	{
		FRAMERATE_1_875, 2,
		FRAMERATE_3_75, 4,
		FRAMERATE_7_5, 8,
		FRAMERATE_15, 15,
		FRAMERATE_30, 30,
		FRAMERATE_60, 60,
		FRAMERATE_120, 120,
		FRAMERATE_240, 240
	};
CFpsMode GetClosestFps(int fps)
{
	int closest=0;;
	int dist=9999999;
	for(int i=1;i<8;++i)
	{
		int d=abs(g_fps[i].fps-fps);
		if(d<dist)
		{
			closest=i;
			dist=d;
		}
	}
	return g_fps[closest];
}
const CFpsMode* GetClosestFps(FrameRate rate)
{
	for(int i=0;i<8;++i)
	{
		if(g_fps[i].f==rate)
			return &g_fps[i];
	}
	return 0;
}

class FlyCameraData
{
public:
	Camera cam;
	FlyCapture2::Image convertedImage;
};

#define CheckError(x) {e=x;if(e!=PGRERROR_OK){FlyCameraManager::getInstance().LogError(e);return false;}}
#define CheckError_NoRet(x) {e=x;if(e!=PGRERROR_OK){FlyCameraManager::getInstance().LogError(e);}}


void FlyCameraImageEventCallback( class Image* pImage, const void* pCallbackData )
{
	FlyCameraVideoGrabber* g = (FlyCameraVideoGrabber*)pCallbackData;
	g->BlitImage((void*)pImage);

}



FlyCameraVideoGrabber::FlyCameraVideoGrabber()
{
	m_inited=false;
	m_format=video::EPixel_R8G8B8;
	m_fps=30;
	m_data=new FlyCameraData();
	FlyCameraManager::getInstance().AddRef();
	m_hasNewFrame = false;

	m_offsetX = m_offsetY = 0;

	m_bufferId = 0;
	m_imageMutex=OS::IThreadManager::getInstance().createMutex();
}
FlyCameraVideoGrabber::~FlyCameraVideoGrabber()
{
	ShutDown();
	delete m_data;
	FlyCameraManager::getInstance().SubRef();
	delete m_imageMutex;
}

void FlyCameraVideoGrabber::ShutDown()
{
}

int FlyCameraVideoGrabber::ListDevices()
{
	return FlyCameraManager::getInstance().GetCamerasCount();
}
core::string FlyCameraVideoGrabber::GetDeviceName(int id)
{
	return core::string("FlyCamera_")+core::StringConverter::toString(id);
}
void FlyCameraVideoGrabber::SetDevice(int id)
{
	InitDevice(id,m_size.x,m_size.y,m_fps);
}

void FlyCameraVideoGrabber::SetFrameRate(int fps)
{
	InitDevice(m_device,m_size.x,m_size.y,fps);
}
int FlyCameraVideoGrabber::GetFrameRate()
{
	return m_fps;
}

bool FlyCameraVideoGrabber::InitDevice(int device,int w,int h,int fps)
{
	m_lastGrabbed=0;
	Stop();
	m_device=device;
	m_fps=fps;
	Error e;
	if(m_inited)
	{
		CheckError_NoRet(m_data->cam.Disconnect());
	}
	m_inited=false;

	PGRGuid guid;
	if (!FlyCameraManager::getInstance().GetCamera(device, guid))
		return false;

	CheckError(m_data->cam.Connect(&guid));

/*
	const PixelFormat k_fmt7PixFmt = PIXEL_FORMAT_RGB8;
	const Mode k_fmt7Mode = MODE_0;
	Format7Info fmt7Info;
	bool supported;
	fmt7Info.mode = k_fmt7Mode;
	e = m_data->cam.GetFormat7Info( &fmt7Info, &supported );

	if ( (k_fmt7PixFmt & fmt7Info.pixelFormatBitField) == 0 )
	{
		// Pixel format not supported!
		printf("Pixel format is not supported\n");
		return -1;
	}


	Format7ImageSettings fmt7ImageSettings;
	fmt7ImageSettings.mode = k_fmt7Mode;
	fmt7ImageSettings.offsetX = 0;
	fmt7ImageSettings.offsetY = 0;
	fmt7ImageSettings.width = w;
	fmt7ImageSettings.height = h;
	fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

	bool valid;
	Format7PacketInfo fmt7PacketInfo;

	e = m_data->cam.ValidateFormat7Settings(
		&fmt7ImageSettings,
		&valid,
		&fmt7PacketInfo );
*/
	//CMode mode=GetClosestVideoMode(w,h);
	CFpsMode fpsMode=GetClosestFps(fps);

	if (true)
	{
		// Query for available Format 7 modes
		Format7Info fmt7Info;
		bool supported;

		if (w<656)
			fmt7Info.mode = MODE_4;
		else
			fmt7Info.mode = MODE_0;
		CheckError(m_data->cam.GetFormat7Info(&fmt7Info, &supported));

//		uint packSize;
//		float perce;
		bool valid;
		Format7PacketInfo fmt7PacketInfo;
		Format7ImageSettings fmt7ImageSettings;
		//CheckError(m_data->cam.GetFormat7Configuration(&fmt7ImageSettings, &packSize,&perce));
		fmt7ImageSettings.mode = fmt7Info.mode;
		if (true)
		{
			fmt7ImageSettings.offsetX = m_offsetX+ (fmt7Info.maxWidth - w) / 2;
			fmt7ImageSettings.offsetY = m_offsetY + (fmt7Info.maxHeight - h) / 2;
			fmt7ImageSettings.width = w;
			fmt7ImageSettings.height = h;
		}
		else
		{
			fmt7ImageSettings.offsetX = 0;
			fmt7ImageSettings.offsetY = 0;

		}
		fmt7ImageSettings.pixelFormat = CMode::GetFormat(m_format);

		// Validate the settings to make sure that they are valid
		CheckError(m_data->cam.ValidateFormat7Settings(
			&fmt7ImageSettings,
			&valid,
			&fmt7PacketInfo));


		CheckError(m_data->cam.GetFormat7Info(&fmt7Info, &supported));

		CheckError(m_data->cam.SetFormat7Configuration(&fmt7ImageSettings, fmt7PacketInfo.recommendedBytesPerPacket));


		Property val;
		val.type = FRAME_RATE;
		val.present = true;
		val.absControl = true;
		val.onePush = false;
		val.onOff = true;
		val.autoManualMode = false;
		val.absValue = m_fps;
		(m_data->cam.SetProperty(&val));


		val.type = GAIN;
		val.autoManualMode = true;
		(m_data->cam.SetProperty(&val));

		val.type = AUTO_EXPOSURE;
		val.autoManualMode = true;
		(m_data->cam.SetProperty(&val));

		val.type = SHUTTER;
		val.autoManualMode = true;
		(m_data->cam.SetProperty(&val));

		val.type = WHITE_BALANCE;
		val.autoManualMode = true;
		(m_data->cam.SetProperty(&val));
		//printf("WB: %f\n", val.absValue);
	}
	else
	{
//		CheckError(m_data->cam.SetVideoModeAndFrameRate(mode.m, fpsMode.f));
//		m_data->cam.GetVideoModeAndFrameRate(&mode.m, &fpsMode.f);
	}

// 	const CMode *pmode=GetClosestVideoMode(mode.m);
// 	const CFpsMode *pfpsMode=GetClosestFps(fpsMode.f);


//	if (pfpsMode)
//	m_fps=frmRate.absValue;
	//if(pmode && ( m_size.x!=pmode->width || m_size.y!=pmode->height))
	{
		m_size.x = w;
		m_size.y = h;

	}
	m_frameCount = 0;
	m_timeAcc = 0;
	m_lastT = 0;
	m_captureFPS = 0;

	m_inited=true;
	return true;

}

void FlyCameraVideoGrabber::SetFrameSize(int w,int h)
{
}
const math::vector2di& FlyCameraVideoGrabber::GetFrameSize()
{
	return m_size;
}

void FlyCameraVideoGrabber::SetImageFormat(video::EPixelFormat fmt)
{
	m_format = fmt;
}
video::EPixelFormat FlyCameraVideoGrabber::GetImageFormat()
{
	return m_format;
}

bool FlyCameraVideoGrabber::GrabFrame(int i)
{
	if(!m_inited)
		return false;
	if(m_hasNewFrame)
	{
		m_imageMutex->lock();
		m_textureImage.setData(m_tempImage.imageData,m_tempImage.Size,m_tempImage.format);

		m_bufferId++;

		m_hasNewFrame=false;
		m_imageMutex->unlock();

		float t = gEngine.getTimer()->getSeconds();
		m_timeAcc += (t - m_lastT)*0.001f;
		++m_frameCount;
		if (m_timeAcc > 1)
		{
			m_captureFPS = m_frameCount;
			m_timeAcc = m_timeAcc - (int)m_timeAcc;
			m_frameCount = 0;
			//	printf("Capture FPS: %d\n", m_captureFPS);
		}

		m_lastT = t;
		return true;
	}
	return false;
	/*
	if(s_videoInput->isFrameNew(m_device))
	{
		m_hasNewFrame=true;
		unsigned char * viPixels = s_videoInput->getPixels(m_device, false,false);


		m_textureImage.setData(viPixels,math::vector3d(m_size.x,m_size.y,1),m_format);

		return true;
	}*/
	return false;
}
bool FlyCameraVideoGrabber::HasNewFrame(int i)
{
	return m_hasNewFrame;
}

void FlyCameraVideoGrabber::Stop()
{
	if(!m_inited)
		return ;
	Error e;
	CheckError_NoRet(m_data->cam.StopCapture());
}
void FlyCameraVideoGrabber::Start()
{
	if(!m_inited)
		return;
	Error e;
	CheckError_NoRet(m_data->cam.StartCapture(FlyCameraImageEventCallback,this));
}
bool FlyCameraVideoGrabber::IsConnected()
{
	if (!m_inited)
		return false;
	return m_data->cam.IsConnected();

}

const video::ImageInfo*  FlyCameraVideoGrabber::GetLastFrame(int i)
{
	return &m_textureImage;
}


void FlyCameraVideoGrabber::BlitImage(const void* img)
{
	float t=gEngine.getTimer()->getSeconds();
// 	if(t-m_lastGrabbed<15)
// 		return;

	Image* pImage = (Image*)img;

	FlyCapture2::PixelFormat pixFormat;
	unsigned int rows, cols, stride;

	// Create a converted image

	EPixelFormat fmt;

	const Image* ptr = pImage;

	if (m_format == EPixel_R8G8B8)
	{
		pImage->Convert(PIXEL_FORMAT_RGB8, &m_data->convertedImage);
		ptr = &m_data->convertedImage;
		fmt = video::EPixel_R8G8B8;

	}
	ptr->GetDimensions(&rows, &cols, &stride, &pixFormat);
	// Convert the raw image
	switch (pixFormat)
	{
	case FlyCapture2::PIXEL_FORMAT_MONO8:
		ptr = pImage;
		fmt = video::EPixel_LUMINANCE8;
		break;
	case FlyCapture2::PIXEL_FORMAT_422YUV8:
	case FlyCapture2::PIXEL_FORMAT_411YUV8:
	case FlyCapture2::PIXEL_FORMAT_444YUV8:
		ptr = pImage;
		fmt = video::EPixel_YUYV;
		break;
	default:
		break;
	}
// 	Image convertedImage;
// 	if(pixFormat!=PIXEL_FORMAT_RGB8)
// 	{
// 	 	pImage->Convert( PIXEL_FORMAT_RGB8, &convertedImage );
// 		fmt = EPixel_R8G8B8;
// 	 	ptr=&convertedImage;
// 	}


	uchar* buf= ptr->GetData();

	m_lastGrabbed=t;
	m_imageMutex->lock();
	{
		m_tempImage.createData(math::vector3d(cols, rows, 1), fmt);
		if (fmt == EPixel_R8G8B8)
		//	video::ColorConverter::convert24BitTo24Bit(buf, m_tempImage.imageData, math::vector2d(rows, cols), 0, false, 0);
		m_tempImage.setData(buf, math::vector3d(cols, rows, 1), fmt);
		else if (fmt == EPixel_LUMINANCE8)
		{
			m_tempImage.setData(buf, math::vector3d(cols, rows, 1), fmt);
		}
		else if (fmt == EPixel_YUYV)
		{
			m_tempImage.setData(buf, math::vector3d(cols, rows , 1), fmt);
		}
	}
	m_hasNewFrame=true;
	m_imageMutex->unlock();

}

void FlyCameraVideoGrabber::SetParameter(const core::string& name, const core::string& value)
{
	Property val;
	val.present = true;
	val.absControl = true;
	val.onePush = false;
	val.onOff = true;
	PropertyInfo ifo;
	if (name == Param_Exposure)
	{
		val.type = AUTO_EXPOSURE;
		float v = core::StringConverter::toFloat(value);
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		if (v < 0 || value == "auto")
			val.autoManualMode = true;
		else {
			v = ifo.absMin + (ifo.absMax - ifo.absMin)*v;
			val.autoManualMode = false;
			val.absValue = v;
		}
		(m_data->cam.SetProperty(&val));
	}
	else
	if (name == Param_Gain)
	{
		val.type = GAIN;
		float v = core::StringConverter::toFloat(value);
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		if (v < 0 || value=="auto")
			val.autoManualMode = true;
		else {
			v = ifo.absMin + (ifo.absMax - ifo.absMin)*v;
			val.autoManualMode = false;
			val.absValue = v;
		}
		(m_data->cam.SetProperty(&val));
	}
	else
	if (name == Param_Gamma)
	{
		val.type = GAMMA;
		float v = core::StringConverter::toFloat(value);
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		if (v < 0 || value == "auto")
			val.autoManualMode = true;
		else {
			v = ifo.absMin + (ifo.absMax - ifo.absMin)*v;
			val.autoManualMode = false;
			val.absValue = v;
		}
		(m_data->cam.SetProperty(&val));
	}
	else
	if (name == Param_Brightness)
	{
		val.type = BRIGHTNESS;
		float v = core::StringConverter::toFloat(value);
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		if (v < 0 || value == "auto")
			val.autoManualMode = true;
		else {
			v = ifo.absMin + (ifo.absMax - ifo.absMin)*v;
			val.autoManualMode = false;
			val.absValue = v;
		}
		(m_data->cam.SetProperty(&val));
	}
	else
	if (name == Param_Saturation)
	{
		val.type = SATURATION;
		float v = core::StringConverter::toFloat(value);
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		if (v < 0 || value == "auto")
			val.autoManualMode = true;
		else {
			v = ifo.absMin + (ifo.absMax - ifo.absMin)*v;
			val.autoManualMode = false;
			val.absValue = v;
		}
		(m_data->cam.SetProperty(&val));
	}
	else
	if (name == Param_WhiteBalance)
	{
		return;	// White Balance is problamatic.. 
		Property tmp;
		val.type = WHITE_BALANCE;
		ifo.type = val.type;
		m_data->cam.GetPropertyInfo(&ifo);
		m_data->cam.GetProperty(&tmp);
		math::vector2d v = core::StringConverter::toVector2d(value);
		if (v.x < 0 || value == "auto")
			val.autoManualMode = true;
		else {
			val.autoManualMode = false;
			val.valueA = ifo.min + (float)(ifo.max - ifo.min)*v.x;
			val.valueB = ifo.min + (float)(ifo.max - ifo.min)*v.y;
		}
		val.absValue = 0;
		val.absControl = false;
		if (tmp.autoManualMode == true && val.autoManualMode == true)
			return;
		(m_data->cam.SetProperty(&val));
	}
}
core::string FlyCameraVideoGrabber::GetParameter(const core::string& name)
{
	Property val;
	val.present = true;
	val.absControl = true;
	val.onePush = false;
	val.onOff = true;
	PropertyInfo ifo;
#define GET_VALUE(t)\
	val.type = t;\
	ifo.type = val.type;\
	m_data->cam.GetPropertyInfo(&ifo);\
	if (m_data->cam.GetProperty(&val) != PGRERROR_OK)\
		return ""; \
	if (val.autoManualMode == true)\
	return "auto";\

	if (name == Param_Exposure)
	{
		GET_VALUE(AUTO_EXPOSURE);
		float v = (val.absValue / (ifo.absMax - ifo.absMin)) - ifo.absMin;
		return core::StringConverter::toString(v);
	}
	else
	if (name == Param_Gain)
	{
		GET_VALUE(GAIN);
		float v = (val.absValue / (ifo.absMax - ifo.absMin)) - ifo.absMin;
		return core::StringConverter::toString(v);
	}
	else
	if (name == Param_Gamma)
	{
		GET_VALUE(GAMMA);
		float v = (val.absValue / (ifo.absMax - ifo.absMin)) - ifo.absMin;
		return core::StringConverter::toString(v);
	}
	else
	if (name == Param_Brightness)
	{
		GET_VALUE(BRIGHTNESS);
		float v = (val.absValue / (ifo.absMax - ifo.absMin)) - ifo.absMin;
		return core::StringConverter::toString(v);
	}
	else
	if (name == Param_Saturation)
	{
		GET_VALUE(SATURATION);
		float v = (val.absValue / (ifo.absMax - ifo.absMin)) - ifo.absMin;
		return core::StringConverter::toString(v);
	}
	else
	if (name == Param_WhiteBalance)
	{
		GET_VALUE(WHITE_BALANCE);
		float va = ((float)val.valueA / (float)(ifo.max - ifo.min)) - ifo.min;
		float vb = ((float)val.valueB / (float)(ifo.max - ifo.min)) - ifo.min;
		return core::StringConverter::toString(math::vector2d(va,vb));
	}
	return "";
}




}
}

