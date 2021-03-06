


#include "stdafx.h"
#include "Application.h"

#include "CMRayApplication.h"
#include "GUIBatchRenderer.h"
#include "GUIManager.h"
#include "ImageSetResourceManager.h"
#include "FontResourceManager.h"
#include "ShaderResourceManager.h"
#include "TextureResourceManager.h"
#include "MeshResourceManager.h"
#include "IFileSystem.h"
#include "GUIThemeManager.h"
#include "GameEntity.h"
#include "GameEntityManager.h"
#include "ViewPort.h"
#include "MeshBufferData.h" 
#include "XMLTree.h"
#include "CFPS.h"
#include "mraynet.h"
#include "IUDPClient.h"
#include "TexLoader.h"
#include "ITexLoader.h"
#include "CMemoryStream.h"
#include "StreamReader.h"
#include "IThreadFunction.h"
#include "IThread.h"
#include "IThreadManager.h"
#include "ParsedShaderPP.h"

#include "PhysicsComponent.h"
#include "SceneComponent.h"
#include "ModelComponent.h"
#include "NetworkMessages.h"

#include "..\\common\\DemoCamera.h"

#include "OptiTrackClient.h"
#include "BitConverter.h"

#include "StringEncoding.h"
#include "SoftwareStreamBuffer.h"
#include "SoftwareIndexBuffer.h"

#include "RenderTechnique.h"
#include "RenderPass.h"
#include "ofSerial.h"

namespace mray
{


	OptiTrackController::OptiTrackController(int id,scene::IMovable*node):m_id(id),m_node(node)
	{
	}
	void OptiTrackController::OnOptiTrackData(animation::OptiTrackClient*client,const animation::OptiTrackRigidBody& body)
	{
		if(m_id==body.GetID())
		{
			m_node->setOrintation(body.GetOrintation());

		}
	}
	CarTrackController::CarTrackController(int id,CarController*node):m_id(id),m_car(node)
	{
		m_initTime=gTimer.getSeconds();
		m_initing=true;
	}
	void CarTrackController::OnOptiTrackData(animation::OptiTrackClient*client,const animation::OptiTrackRigidBody& body)
	{
		if(m_id==body.GetID())
		{
			//m_node->setPosition(body.GetPosition());

			math::vector3d angles;
			angles.x=body.GetOrintation().getPitch();
			angles.y=body.GetOrintation().getYaw();
			if(m_initing)
			{
				if(gTimer.getSeconds()-m_initTime>2000)
				{
					m_initing=false;
					m_headEstimator.Calculate();
					m_rotateEstimator.Calculate();
				}else
				{
					m_headEstimator.AddSample(body.GetPosition());
					m_rotateEstimator.AddSample(angles);
				}
			}else
			{
				math::quaternion q= m_rotateEstimator.Mean();
				math::vector3d diff=-(body.GetPosition()-m_headEstimator.Mean());
				diff.x*=2;
				float len=diff.Length();
				diff/=len;
				len=pow(len*0.5f,2.0f)*1000.0f;
				len=math::clamp(len,0.0f,100.0f);
				diff*=len;
				diff=q.inverse()*diff;

				if(diff.Length()>m_headEstimator.Variance().Length() && m_lastSpeed.getDistSQ(diff)>0.1)
				{
					m_lastSpeed=diff;
					//diff/=m_headEstimator.Variance();
				
					m_car->SetSpeed(math::vector2d(diff.x,diff.z));
				}
				//angles=(angles-m_rotateEstimator.Mean());
				if(m_lastAngles.getDistSQ(angles)>5)
				{
					m_lastAngles=angles;
					m_car->SetLookingDirection(angles);
				}
			}

		}
	}
	class CameraVideoReceiverThread:public OS::IThreadFunction
	{
		GCPtr<network::IUDPClient> m_udpClient;
		Application* m_app;
	public:
		CameraVideoReceiverThread(Application* app,GCPtr<network::IUDPClient> client)
		{
			m_app=app;
			m_udpClient=client;
		}
		virtual void setup()
		{
		}
		virtual void execute(OS::IThread*caller,void*arg)
		{
			byte buffer[4096*10];
			uint bufLen;
			GCPtr<video::ImageInfo> image=new video::ImageInfo();
			OS::CMemoryStream memStream("",(byte*)buffer,sizeof(buffer),false);
			network::NetAddress recAddr;
			loaders::ITexLoader* loader= gTextureResourceManager.GetLoaders()->getLoader("jpg");
			while(caller->isActive())
			{
				bufLen=sizeof(buffer);
				m_udpClient->RecvFrom((char*)buffer,&bufLen,&recAddr);
				if(bufLen>0)
				{
					memStream.setData(buffer,bufLen);
					NetworkMessage* msg=NetworkMessage::DeserializePacket(OS::StreamReader(&memStream));
					if(msg)
					{
						VideoImageMessage* m=(VideoImageMessage*)msg;
						m_app->_NewImageRecieved(m->GetImage());
						delete msg;
					}
				}
			}
		}
	};

	Application::Application(){}
	Application::~Application()
	{
		m_videoTex=0;
		m_imageProcessor=0;
	}


	game::GameEntity* Application::CreateGameEntity(const core::string& modelPath,const core::string& collision,const math::vector3d& pos)
	{
		scene::ISceneNode* sceneNode;
		game::GameEntity* gameEnt;
		physics::IPhysicalNode* phNode=0;
		physics::PhysicalNodeDesc phNodeDesc;


		physics::PhysicMaterialDesc matDesc;

		matDesc.dynamicFriction=0.9;
		matDesc.staticFriction=0.9;
/*

		xml::XMLTree tree;
		if(tree.load(gFileSystem.openFile(collision)))
		{
			phNodeDesc.LoadFromXML(tree.getSubElement("PhysicalNode"));

			phNodeDesc.globalPos.setTranslation(pos);//math::vector3d(zoffset,100,offset));
			phNode=m_phManager->createNode(&phNodeDesc);
		}*/


		sceneNode=getSceneManager()->createSceneNode();
		gameEnt=m_gameManager->CreateGameEntity(modelPath);
		game::SceneComponent* sComp=new game::SceneComponent(m_gameManager);
		sComp->SetName("Body");
		game::PhysicsComponent* phComp=new game::PhysicsComponent(m_gameManager);
		gameEnt->AddComponent(sComp);
		gameEnt->AddComponent(phComp);
		sComp->SetSceneNode(sceneNode);
		phComp->SetPhysicsModel(collision);
//		phComp->SetTargetName("Body");
		phComp->SetName("Body");
		phComp->SetPosition(pos);

		game::ModelComponent* mComp=new game::ModelComponent();

		mComp->SetModelPath(modelPath);
		sComp->AddComponent(mComp);
		scene::MeshRenderableNode* modelComp=mComp->GetModel();
		scene::IMeshBuffer* buff= modelComp->getMesh()->getBuffer(0);
		//buff->setRenderType(video::MR_POINTS);

		phComp->InitComponent();


//  		sceneNode->setVisible(false);
//  		_AddInstanceModel(modelComp);
	//	scene::SMeshManipulator::ConvertTrisToTriStrips(buff);

		return gameEnt;
	}

	void Application::onEvent(Event* event)
	{
		CMRayApplication::onEvent(event);
		if(m_guiManager)
			m_guiManager->OnEvent(event);

		if(event->getType()==ET_ResizeEvent)
		{
			ResizeEvent* e=(ResizeEvent*)event;
		}
		if(event->getType()==ET_Mouse)
		{
			MouseEvent* e=(MouseEvent*)event;
		}
		if(event->getType()==ET_Keyboard)
		{
			static math::vector2d speed;
			KeyboardEvent*e=(KeyboardEvent*) event;
			if(e->press && e->key==KEY_LEFT)
			{
				speed.x+=5;
				speed.x=math::clamp(speed.x,-100.0f,100.0f);
				m_carController->SetSpeed(speed);
			}
			if(e->press && e->key==KEY_RIGHT)
			{
				speed.x-=5;
				speed.x=math::clamp(speed.x,-100.0f,100.0f);
				m_carController->SetSpeed(speed);
			}

			if(e->press && e->key==KEY_UP)
			{
				speed.y+=5;
				speed.y=math::clamp(speed.y,-100.0f,100.0f);
				m_carController->SetSpeed(speed);
			}
			if(e->press && e->key==KEY_DOWN)
			{
				speed.y-=5;
				speed.y=math::clamp(speed.y,-100.0f,100.0f);
				m_carController->SetSpeed(speed);
			}
		}
	}

	void Application::CreatePhysicsSystem()
	{
		physics::PhysicsSystemDesc desc;
		desc.gravity.set(0,-98,0);
		m_phManager=new physics::PhysXManager(&desc);
		m_phManager->ConnectToRemoteDebugger();
	}
	void Application::_AddInstanceModel(scene::MeshRenderableNode* buff)
	{
 		buff->setMaterial(gMaterialResourceManager.getMaterial("InstanceMaterial"),0);
// 		InstanceMap::iterator it= m_instances.find(buff->getMesh());
// 		if(it==m_instances.end())
		if(m_instances.begin()==m_instances.end() || m_instances.back()->getInstancesCount()>=100)
		{
			scene::VertConstGeoInstancing* inst=new scene::VertConstGeoInstancing();
			scene::ISceneNode* node= getSceneManager()->createSceneNode();
			scene::MeshRenderableNode* r=new scene::MeshRenderableNode(inst->getMesh());
			node->AttachNode(r);
			m_instances.push_back(inst);
		}
		m_instances.back()->addNode(buff);
	}

	void Application::init(const OptionContainer &extraOptions)
	{
		CMRayApplication::init(extraOptions);

		CMRayApplication::loadResourceFile(mT("Resources.stg"));

		m_grabber=new video::DirectShowVideoGrabber();
		m_grabber->InitDevice(0,320,240,30);

		{
			gImageSetResourceManager.loadImageSet(mT("VistaCG.imageset"));
			GCPtr<OS::IStream> themeStream=gFileSystem.createBinaryFileReader(mT("VistaCG.xml"));
			GUI::GUIThemeManager::getInstance().loadTheme(themeStream);
			GUI::GUIThemeManager::getInstance().setActiveTheme(mT("VistaCG"));
		}
		{
			//load font
			GCPtr<GUI::IFont>font=gFontResourceManager.getOrCreate(mT("solo5_font.xml"));
			gFontResourceManager.setDefaultFont(font);
		}
		m_videoTex=gTextureResourceManager.createEmptyTexture2D("",true);

		if(0)
		{


			m_network=network::createWin32Network();
			m_udpClient=m_network->createUDPClient();
			m_udpClient->Open();
			m_udpClient->Connect(network::NetAddress("127.0.0.1",1213));

			byte buffer[1];
			buffer[0]=100;
			m_udpClient->SendTo(0,(char*)buffer,1);

			CameraVideoReceiverThread* tfunc=new CameraVideoReceiverThread(this,m_udpClient);
			m_videoThread=OS::IThreadManager::getInstance().createThread(tfunc);
			m_videoThread->start(0);
		}


		m_carController=new CarController("COM13");
		m_carController->SetSpeed(math::vector2d(0,0));

		m_renderShader=new video::GPUShader();
		m_renderShader->SetVertexShader(gShaderResourceManager.loadShader("instPhong.cg",video::EShader_VertexProgram,"main_vp","cg"));
		m_renderShader->SetFragmentShader(gShaderResourceManager.loadShader("instPhong.cg",video::EShader_FragmentProgram,"main_fp","cg"));


		video::RenderMaterial* mtrl=new video::RenderMaterial();
		
		mtrl->CreateTechnique("")->CreatePass("")-> setRenderShader(m_renderShader);
		gMaterialResourceManager.addResource(mtrl,"InstanceMaterial");

		m_guiRender=new GUI::GUIBatchRenderer();
		m_guiRender->SetDevice(getDevice());


		m_guiManager=new GUI::GUIManager(getDevice());

		m_gameManager=new game::GameEntityManager();

		CreatePhysicsSystem();
		m_gameManager->SetPhysicsManager(m_phManager);
		m_gameManager->SetSceneManager(getSceneManager());

		//////////////////////////////////////////////////////////////////////////
		// Create Scene

		m_camera=new DemoCamera(getSceneManager(),10,GetInputManager());
		m_viewPort=GetRenderWindow()->CreateViewport(mT("MainVP"),m_camera,0,math::rectf(0,0,1,1),0);
		getSceneManager()->addSceneNode(m_camera);
		m_camera->setPosition(math::vector3d(0,10,-10));

		xml::XMLTree tree;

		if(tree.load(gFileSystem.openFile("VTelesar.xml",OS::BIN_READ)))
		{
			m_gameManager->loadXMLSettings(&tree);
		}

#if 0

		game::GameEntity* ent= CreateGameEntity("robotGround.mdx","robotGround.phys",0);

		if(true)
		{
			xml::XMLElement* rootE=new xml::XMLElement("GameEntities");

			int i=0,j=0;

			//for (int i=-1;i<1;++i)
			{
				//for(int j=-1;j<1;++j)
				{
					m_robot= CreateRobot(math::vector3d(25*i,5,25*j));
 					//m_robot->SetMaxSpeed(300);
					//m_robots.push_back(m_robot);
					m_robot->GetOwnerEntity()-> exportXMLSettings(rootE);
				}
			}
			{
				//m_robot->GetOwnerEntity()->exportXMLSettings(rootE);

				xml::XMLWriter w;
				w.addElement(rootE);
				core::string xmlStr= w.flush();
				OS::StreamWriter wrtr( gFileSystem.openFile("ents.xml",OS::BIN_WRITE));
				wrtr.writeString(xmlStr);
				wrtr.getStream()->close();
				delete rootE;
			}
		}else
		{
			xml::XMLTree tree;
			if(tree.load(gFileSystem.openFile("ents.xml",OS::BIN_READ)))
			{
				m_gameManager->loadXMLSettings(tree.getSubElement("GameEntities"));
				game::GameEntity*ent= m_gameManager->GetGameEntity(2);
			}
		}

		InstanceMap::iterator it=m_instances.begin();
		for(;it!=m_instances.end();++it)
		{
			(*it)->build();
		}
#endif
		//((game::SceneComponent*)m_robot->GetOwnerEntity()->GetFirstComponent("SceneComponent"))
		//	->GetSceneNode()->addChild(m_camera);
		m_camera->setPosition(math::vector3d(0,100,0));



		if(1)
		{
			GCPtr<scene::LightNode> m_light=getSceneManager()->createLightNode(mT(""));

			m_light->setRadius(1000);
			m_light->setDiffuse(video::SColor(0.7,0.5,0.5,1));
			m_light->setAmbient(video::SColor(0.5,0.5,0.5,1));
			m_light->setType(scene::LT_PointLight);

			m_light->setPosition(math::vector3d(10,10,10));
			GCPtr<video::ITexture> texture=getDevice()->createEmptyTexture2D(true);
			texture->setMipmapsFilter(false);
			texture->SetNumberOfMipmaps(0);
			texture->createTexture(1024,video::EPixel_Float16_R);
			GCPtr<video::IRenderTarget> shadowRT=getDevice()->createRenderTarget(mT("shadowRenderTarget"),texture,0,0,0);

			m_light->setCastShadows(true);
			m_light->setShadowMap(shadowRT);

			math::matrix4x4 m_projection=math::MathUtil::CreateProjectionMatrixPerspectiveFov(45,1,1,200);
			m_light->setProjection(m_projection);
			m_light->update(0);

		}

		if(0)
		{
			GCPtr<video::ITexture> tex= scene::SkyBoxManager::getInstance().loadSkyFromFolder(mT("skybox\\night\\"),mT("png"),mT("night"));
			GCPtr<scene::IRenderable> sky=new scene::SSkyBoxNode(tex);//scene::SceneNodeCreator::addSkyBox(getSceneManager(),tex);
			//sky->SetHasCustomRenderGroup(true);
			//sky->SetTargetRenderGroup(scene::RGH_Skies);
			scene::ISceneNode*node= getSceneManager()->createSceneNode();
			node->AttachNode(sky);
			node->setCullingType(scene::SCT_NONE);

		}


		{
			m_imageProcessor=new video::CMultiPassPP(getDevice());
			video::ParsedShaderPP* pp=new video::ParsedShaderPP(getDevice());
			pp->LoadXML(gFileSystem.openFile(mT("blur.peff")));
			m_imageProcessor->addPostProcessor(pp);

			video::ITexturePtr rt=getDevice()->createTexture2D(math::vector2di(640,480),video::EPixel_LUMINANCE8,true);
			m_targetImage=getDevice()->createRenderTarget(mT(""),rt,0,0,0);
			m_imageProcessor->Setup(math::rectf(0,0,rt->getSize().x,rt->getSize().y));

			int w=64;
			int h=48;

			m_mbuffer=new scene::SMeshBuffer();
			m_mbuffer->createStream(0,video::EMST_Position,video::ESDT_Point3f,w*h,video::IHardwareBuffer::EUT_Dynamic,false,true);
			m_mbuffer->createStream(0,video::EMST_Color,video::ESDT_Point4f,w*h,video::IHardwareBuffer::EUT_Dynamic,false,true);
			m_mbuffer->createIndexBuffer(video::IHardwareIndexBuffer::EIT_16Bit,(w-1)*(h-1)*6,video::IHardwareBuffer::EUT_Dynamic,false,false);
			ushort*idc=(ushort*)m_mbuffer->getIndexData()->indexBuffer->lock(0,0,video::IHardwareBuffer::ELO_Discard);
			ushort*ptr=idc;
			for(int j=0;j<w-1;++j)
			{
				for(int i=0;i<h-1;++i)
				{
					*(idc++)=j*h+i;
					*(idc++)=(j+1)*h+i;
					*(idc++)=(j)*h+i+1;
					*(idc++)=(j+1)*h+i;
					*(idc++)=(j+1)*h+i+1;
					*(idc++)=(j)*h+i+1;
				}
			}
		//	for(int i=0;i<10;++i)
		//		printf("%d\n",ptr[i]);
			m_mbuffer->setRenderType(video::MR_TRIANGLES);
			m_mbuffer->getIndexData()->indexBuffer->unlock();

		}

		if(0)
		{
			video::ParsedShaderPP* pp=new video::ParsedShaderPP(getDevice());
			pp->LoadXML(gFileSystem.openFile(mT("MotionBlur.peff")));
			scene::ViewPort* vp[]={m_viewPort};
			for(int i=0;i<1;++i)
			{

				vp[i]->enablePostProcessing(true);
				vp[i]->setPostProcessing(pp);

				video::ITexturePtr renderTargetTex=getDevice()->createTexture2D(vp[i]->getAbsViewPort().getSize(),video::EPixel_Float16_RGB,true);

				video::IRenderTargetPtr rt=getDevice()->createRenderTarget(mT(""),renderTargetTex,video::IHardwareBufferPtr::Null,video::IHardwareBufferPtr::Null,false);
				renderTargetTex=getDevice()->createTexture2D(vp[i]->getAbsViewPort().getSize(),video::EPixel_Float16_RGB,true);
				rt->attachRenderTarget(renderTargetTex,1);

				vp[i]->setRenderTarget(rt);

			}
		}
		//m_camera->setVisible(false);
		//////////////////////////////////////////////////////////////////////////
		m_optiTrack=new animation::OptiTrackClient();
		m_optiTrack->Connect(mT("192.168.10.129"),mT("192.168.10.129"),mT("239.255.42.99"));
		m_optiCarController=new CarTrackController(0,m_carController);

// 		m_camController=new OptiTrackController(0,m_camera);
		m_optiTrack->AddListener(this);
		m_optiTrack->AddListener(m_optiCarController);

	}

	void Application::draw(scene::ViewPort* vp)
	{

		CMRayApplication::draw(vp);
		getDevice()->set2DMode();
		if(1)
		{
			video::TextureUnit tu;
			getDevice()->setRenderTarget(m_targetImage);
			if(m_grabber->HasNewFrame())
			{
				const video::ImageInfo* ifo[]={m_grabber->GetLastFrame()};
				m_videoTex->loadSurfaces(ifo,1);
			}
			tu.SetTexture(m_videoTex);
			getDevice()->useTexture(0,&tu);
			getDevice()->draw2DImage(math::rectf(0,0,m_targetImage->getSize().x,m_targetImage->getSize().y),video::SColor(1,1,1,1));
			getDevice()->setRenderTarget(0);

			m_imageProcessor->render(m_targetImage);
			video::IHardwarePixelBuffer* surf=m_imageProcessor->getOutput()->getColorTexture()->getSurface(0);
			video::LockedPixelBox box= surf->lock(math::box3d(0,m_imageProcessor->getOutput()->getColorTexture()->getSize()),video::IHardwareBuffer::ELO_ReadOnly);
			/*
			tu.SetTexture(m_imageProcessor->getOutput()->getColorTexture());
			getDevice()->useTexture(0,&tu);
			getDevice()->draw2DImage(math::rectf(0,0,m_targetImage->getSize().x,m_targetImage->getSize().y),video::SColor(1,1,1,1));
			*/
			video::IHardwareStreamBuffer* vbuff= m_mbuffer->getStream(0,video::EMST_Position);
			video::IHardwareStreamBuffer* cbuff= m_mbuffer->getStream(0,video::EMST_Color);
			video::SColor* clr=(video::SColor*)cbuff->lock(0,0,video::IHardwareBuffer::ELO_Discard);
			math::vector3d* vert=(math::vector3d*)vbuff->lock(0,0,video::IHardwareBuffer::ELO_Discard);

			float rw=box.box.getWidth()/64.0f;
			float rh=box.box.getHeight()/48.0f;
			//printf("%f %f %f %f\n",box.box.getWidth(),box.box.getHeight(),rw,rh);
			for(float i=0,x=0;i<box.box.getWidth();i+=rw,++x)
			//for(int x=0;x<64;++x)
			{
				for(float j=0,y=0;j<box.box.getHeight();j+=rh,++y)
				//for(int y=0;y<48;++y)
				{
					int idx=j*box.box.getWidth()+i;
					float z=((uchar*)box.data)[idx]/256.0f;
					(*vert).set(x,z*40,y);
					(*clr).Set(z,z,z,1);
					++vert;
					++clr;
				}
			}
			vbuff->unlock();
			cbuff->unlock();
			video::RenderPass mtrl(0);
			mtrl.setRenderState(video::RS_CullFace,video::ES_DontUse);
			mtrl.setRenderState(video::RS_Lighting,video::ES_DontUse);
			getDevice()->set3DMode();
			getDevice()->useRenderPass(&mtrl);
			getDevice()->drawSingleMesh(m_mbuffer);
			getDevice()->useRenderPass(0);
			getDevice()->set2DMode();
		}
		GCPtr<GUI::IFont> font=gFontResourceManager.getDefaultFont();
		if(font){
			m_guiRender->Prepare();

			float yoffset=50;


			GUI::FontAttributes attr;
			attr.fontColor.Set(0.05,1,0.5,1);
			attr.fontAligment=GUI::EFA_MiddleLeft;
			attr.fontSize=20;
			attr.hasShadow=true;
			attr.shadowColor.Set(0,0,0,1);
			attr.shadowOffset=math::vector2d(2);
			attr.spacing=2;
			attr.wrap=0;
			attr.RightToLeft=0;
			core::string msg=mT("FPS= ");
			msg+=core::StringConverter::toString(core::CFPS::getInstance().getFPS());
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;
			 msg=mT("Capture FPS= ");
			msg+=core::StringConverter::toString(m_optiTrack->GetSamplesPerSecond());
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;
			 msg=mT("Primitives= ");
			msg+=core::StringConverter::toString(getDevice()->getPrimitiveDrawnCount());
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;
			msg=mT("Batches= ");
			msg+=core::StringConverter::toString(getDevice()->getBatchDrawnCount());
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;

			msg=mT("Speed= ");
			msg+=core::StringConverter::toString(m_optiCarController->m_lastSpeed);
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;

			msg=mT("Angles= ");
			msg+=core::StringConverter::toString(m_optiCarController->m_lastAngles);
			font->print(math::rectf(10,yoffset+10,10,10),&attr,0,msg,m_guiRender);
			yoffset+=attr.fontSize;


			m_guiRender->Flush();
			getDevice()->useShader(0);
		}
		m_guiManager->DrawAll(vp);

	}
	void Application::WindowPostRender(video::RenderWindow* wnd)
	{
	}
	void Application::update(float dt)
	{
		CMRayApplication::update(dt);

//		m_grabber->GrabFrame(dt);
		m_phManager->update(dt);
		m_gameManager->Update(dt);

		InstanceMap::iterator it=m_instances.begin();
		for(;it!=m_instances.end();++it)
		{
			(*it)->update();
		}
	}

	void Application::onDone()
	{
		CMRayApplication::onDone();
		if(m_udpClient)
			m_udpClient->Close();
		if(m_videoThread)
			m_videoThread->terminate();
	}

	void Application::_NewImageRecieved(video::ImageInfo* image)
	{
		const video::ImageInfo* img[]={image};
		m_videoTex->loadSurfaces(img,1);
	}

	void Application::OnOptiTrackData(animation::OptiTrackClient*client,const animation::OptiTrackRigidBody& body)
	{
		if(0==body.GetID())
		{
			//m_node->setPosition(body.GetPosition());
			m_camera->setOrintation(body.GetOrintation());


		}
	}
}

