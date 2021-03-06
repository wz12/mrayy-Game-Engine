
/********************************************************************
	created:	2009/04/22
	created:	22:4:2009   19:35
	filename: 	i:\Programing\GameEngine\mrayEngine\EaglesProject\src\GlarePP.h
	file path:	i:\Programing\GameEngine\mrayEngine\EaglesProject\src
	file base:	GlarePP
	file ext:	h
	author:		Mohamad Yamen Saraiji
	
	purpose:	
*********************************************************************/

#ifndef ___GlarePP___
#define ___GlarePP___

#include <IPostProcessing.h>
#include "ShaderCallback.h"

namespace mray{

class GlarePP:public video::IPostProcessing
{
private:

	video::IVideoDevice*device;
public:

	static const int maxLuminance=4;

	GCPtr<video::IRenderTarget> m_blurRT[2];
	GCPtr<video::IRenderTarget> m_finalRT;
	GCPtr<video::IRenderTarget> m_lumRT[maxLuminance];


	GCPtr<video::TextureUnit> m_renderUT;
	GCPtr<video::TextureUnit> m_blurTU[2];
	GCPtr<video::TextureUnit> m_finalTU;
	GCPtr<video::TextureUnit> m_lumTU[maxLuminance];

	GCPtr<video::IGPUShaderProgram> m_lum;
	GCPtr<video::IGPUShaderProgram> m_uBlur;
	GCPtr<video::IGPUShaderProgram> m_vBlur;
	GCPtr<video::IGPUShaderProgram> m_final;
	GCPtr<PostprocessingShaderConstants> m_callback;
public:

	GlarePP(video::IVideoDevice*dev,const math::vector2d &size);
	virtual~GlarePP();

	virtual video::IRenderTarget* render(video::IRenderTarget* input);
	virtual video::IRenderTarget* getOutput();
};

}


#endif //___GlarePP___
