

/********************************************************************
	created:	2014/04/10
	created:	10:4:2014   22:21
	filename: 	C:\Development\mrayEngine\Projects\TEDxTokyo\TwitterVisualizer\TwitterProfileListItem.h
	file path:	C:\Development\mrayEngine\Projects\TEDxTokyo\TwitterVisualizer
	file base:	TwitterProfileListItem
	file ext:	h
	author:		MHD Yamen Saraiji
	
	purpose:	
*********************************************************************/

#ifndef __TwitterProfileListItem__
#define __TwitterProfileListItem__

#include "IListItem.h"
#include "TextureUnit.h"

namespace mray
{
	namespace kmd
	{
		class KMDUser;
	}
namespace GUI
{

class TwitterProfileListItem:public IListItem
{
protected:
	kmd::KMDUser* m_user;
	video::ITexturePtr m_profilePic;
public:
	TwitterProfileListItem(kmd::KMDUser* user);
	virtual~TwitterProfileListItem();

	kmd::KMDUser* GetUser(){ return m_user; }

	virtual core::UTFString toString()const;

	virtual void Draw(const math::rectf& rc, IGUIElement* lstBox, IFont* font, IGUIRenderer* renderer) ;

};

}
}


#endif
