
#include "stdafx.h"
#include "LatencyTestState.h"
#include "ATAppGlobal.h"
#include "TBRobotInfo.h"
#include "RobotInfoManager.h"
#include "DataCommunicator.h"
#include "RemoteRobotCommunicator.h"
#include "IFileSystem.h"
#include "StreamWriter.h"
#include "GUIBatchRenderer.h"
#include "FontResourceManager.h"
#include "TextureRTWrap.h"
#include "INetwork.h"


namespace mray
{
namespace AugTel
{

LatencyTestState::LatencyTestState(const core::string& name, TBee::ICameraVideoSource* src)
:IRenderingState(name)
{
	m_camVideoSrc = src;
	m_showColor = false;
	m_robotConnector = new CRobotConnector();
	m_robotConnector->SetCommunicator(new TBee::RemoteRobotCommunicator());
	m_minLatency = 99999;
	m_maxLatency = 0;

	m_targetAddress = "127.0.0.1";
	m_targetPort = 5123;

	m_I420ToRGB = new video::ParsedShaderPP(Engine::getInstance().getDevice());
	m_I420ToRGB->LoadXML(gFileSystem.openFile("I420ToRGB.peff"));
	m_autoTesterTimer = 0;

}
LatencyTestState::~LatencyTestState()
{
	if (m_outValues)
		m_outValues->close();
	delete m_outValues;
	delete m_camVideoSrc;
	delete m_robotConnector;
}

void LatencyTestState::InitState()
{
	IRenderingState::InitState();
	m_camVideoSrc->Init();

	GUI::GUIBatchRenderer*r = new GUI::GUIBatchRenderer();
	r->SetDevice(Engine::getInstance().getDevice());
	m_guiRenderer = r;
	m_client = network::INetwork::getInstance().createUDPClient();
	m_client->Open();
}
bool LatencyTestState::OnEvent(Event* e, const math::rectf& rc)
{
	bool ok = false;
	if (e->getType() == ET_Keyboard)
	{
		KeyboardEvent* evt = (KeyboardEvent*)e;
		if (evt->press)
		{
			if (evt->key == KEY_SPACE)
			{
				_OnChange(!m_showColor);

				if (m_showColor)
				{
					m_changed = false;
					m_startTime = gEngine.getTimer()->getSeconds();
				}
				ok = true;
			}
			else if (evt->key == KEY_ESCAPE)// && evt->lshift)
			{
				m_exitCode = STATE_EXIT_CODE;
				ok = true;
			}
		}
	}
	return ok;
}
void LatencyTestState::OnEnter(IRenderingState*prev)
{
	IRenderingState::OnEnter(prev);
	m_camVideoSrc->Open();
	m_latency.clear();
	m_changed = false;

	

	gAppData.dataCommunicator->Start(gAppData.TargetCommunicationPort);

	TBee::TBRobotInfo* ifo = gAppData.selectedRobot;
	if (ifo)
		m_robotConnector->ConnectRobotIP(ifo->IP, gAppData.TargetVideoPort, gAppData.TargetAudioPort, gAppData.TargetHandsVideoPort,gAppData.TargetClockPort, gAppData.TargetCommunicationPort, gAppData.RtcpStream);
	//m_robotConnector->EndUpdate();
	m_robotConnector->ConnectRobot();

	m_outValues = gFileSystem.createBinaryFileWriter(gFileSystem.getAppPath() + "\\Latency\\Latency.txt");

	if (ifo)
		m_targetAddress = ifo->IP;

	m_remoteAddr.setIP(m_targetAddress);
	m_remoteAddr.port = m_targetPort;
}
void LatencyTestState::OnExit()
{
	IRenderingState::OnExit();
	m_camVideoSrc->Close();
	m_robotConnector->DisconnectRobot();
	m_outValues->close();
}
video::IRenderTarget* LatencyTestState::Render(const math::rectf& rc, TBee::ETargetEye eye)
{

	video::IRenderTarget* rt = IRenderingState::Render(rc, eye);
	video::TextureUnit tex;
	m_camVideoSrc->Blit(0);

	Engine::getInstance().getDevice()->setRenderTarget(rt,0,1,1);
	if (m_showColor)
	{
		Engine::getInstance().getDevice()->draw2DRectangle(rc, video::SColor(0, 0, 0, 1));

	}
	else
	{
		Engine::getInstance().getDevice()->draw2DRectangle(rc, video::SColor(1, 1, 1, 1));

	}

	video::ITexturePtr cameraTex = m_camVideoSrc->GetEyeTexture(0);
	math::vector2d size(cameraTex->getSize().x, cameraTex->getSize().y);
	if (false)
	{
		m_I420ToRGB->Setup(math::rectf(0, size));
		m_I420ToRGB->render(&video::TextureRTWrap(cameraTex));
		cameraTex = m_I420ToRGB->getOutput()->GetColorTexture();

	}
	tex.SetTexture(cameraTex);
	if (!tex.GetTexture()->getSurfaceCount())
		return rt;
	video::IHardwarePixelBuffer* s = tex.GetTexture()->getSurface(0);
	video::LockedPixelBox pbb = s->lock(math::box3d(s->getWidth() / 2, s->getHeight()/2, 0, 1,1, 1), video::IHardwareBuffer::ELO_ReadOnly);

	struct CRgb
	{
		uchar r, g, b;
	};

	CRgb* rgb = ((CRgb*)pbb.data);// +(s->getWidth()*s->getHeight() + s->getWidth()) / 2;
	video::SColor clr(rgb->r*math::i255, rgb->g*math::i255, rgb->b*math::i255, 1);
	Engine::getInstance().getDevice()->draw2DRectangle(math::rectf(0, 100, 100, 200), clr);

	s->unlock();


	float t = gEngine.getTimer()->getSeconds();
	float dt = t - m_lastTime;
	m_lastTime = t;


	if (m_showColor && !m_changed)
	{
		//if (clr.R > 0.5 && clr.G < 0.6 && clr.B < 0.6)
		if (clr.R < 0.5 )
		{
			m_changed = true;
			float latency = gEngine.getTimer()->getSeconds() - m_startTime;
			latency -= dt;

			m_minLatency = math::Min<float>(m_minLatency, latency);
			m_maxLatency = math::Min<float>(m_maxLatency, latency);
			printf("Latency[%d]= %f ms\n", m_latency.size(), latency);
			m_latency.push_back(latency);
			if (m_outValues)
			{
				OS::StreamWriter wrtr(m_outValues);
				wrtr.writeLine(core::StringConverter::toString(latency));
			}
		}
	}

	Engine::getInstance().getDevice()->useTexture(0, &tex);
	Engine::getInstance().getDevice()->draw2DImage(math::rectf(0, 0, s->getWidth(),s->getHeight()), video::SColor(1, 1, 1, 1));

	if (m_latency.size() > 1)
	{
		float x = 100;
		float y = 200;
		std::vector<math::vector2d> points;
		points.resize(m_latency.size());

		for (int i = 0; i < m_latency.size() ; ++i)
		{
			float l1 = m_latency[i];
			l1 = (l1 - m_minLatency) / (m_maxLatency - m_minLatency);
			points[i].set(x, y + l1);
			
			x += 10;
		}

		Engine::getInstance().getDevice()->draw2DLine(&points[0],points.size(),1);
	}
	GUI::IFont* font = gFontResourceManager.getDefaultFont();
	GUI::FontAttributes attr;
	video::IVideoDevice* dev = Engine::getInstance().getDevice();
	dev->set2DMode();
	if (font)
	{
#define PRINT_LOG(txt)\
	msg = txt; \
	font->print(r, &attr, 0, msg, m_guiRenderer); \
	r.ULPoint.y += attr.fontSize + attr.fontSize;

		attr.fontColor.Set(1, 1, 1, 1);
		attr.fontAligment = GUI::EFA_MiddleLeft;
		attr.fontSize = 18;
		attr.hasShadow = true;
		attr.shadowColor.Set(0, 0, 0, 1);
		attr.shadowOffset = math::vector2d(2);
		attr.spacing = 2;
		attr.wrap = 0;
		attr.RightToLeft = 0;
		core::string msg;
		math::rectf r = rc;
		r.ULPoint = math::vector2d(250,100);

		PRINT_LOG((mT("Capture Frame Rate:") + core::StringConverter::toString(m_camVideoSrc->GetCaptureFrameRate(0))));
		PRINT_LOG((mT("Update Latency:") + core::StringConverter::toString(dt)));
		PRINT_LOG("");
		PRINT_LOG("");


		attr.fontSize = 40;
		PRINT_LOG((core::StringConverter::toString(gEngine.getTimer()->getMilliseconds())));

	}
	m_guiRenderer->Flush();
	return rt;
}
void LatencyTestState::Update(float dt)
{
	m_robotConnector->UpdateStatus();
	
	float t = gEngine.getTimer()->getSeconds();
	if (t - m_autoTesterTimer > 600)
	{
		if (m_showColor && m_changed)
		{
			_OnChange(false);
		}
		else if (!m_showColor && m_changed)
		{
			_OnChange(true);
			m_changed = false;
			m_startTime = t;
		}
		m_autoTesterTimer = t;


	}

}

void LatencyTestState::_OnChange(bool c)
{
	m_showColor = c;

	const int BufferLen = 64;
	uchar buffer[BufferLen];
	OS::CMemoryStream stream("", buffer, BufferLen, false, OS::BIN_WRITE);
	int len = stream.write(&m_showColor, sizeof(m_showColor));
	m_client->SendTo(&m_remoteAddr, (char*)buffer, len);
}



}
}

