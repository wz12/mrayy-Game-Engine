#include "IGUISchemeBase.h"
#include "GUIPanel.h"
#include "GUIProfilePicture.h"
#include "GUIStaticImage.h"
#include "GUIStaticText.h"
namespace mray{

using namespace GUI;
class GUISpeakerDetailsPanelLayout:public GUI::IGUISchemeBase
{

public:
	GUIPanel* Root;
	GUIPanel* Contents;
	GUIStaticText* SessionName;
	GUIStaticText* SessionStartTime;
	GUIStaticText* SessionEndTime;
	GUIStaticText* SpeakerID;
	GUIStaticText* TwitterID;
	GUIProfilePicture* SpeakerImage;
	GUIStaticText* SpeakerTitle;
	GUIStaticText* SpeakerName;
	GUIStaticImage* TEDxLogo;

public:

	GUISpeakerDetailsPanelLayout():Root(0),Contents(0),SessionName(0),SessionStartTime(0),SessionEndTime(0),SpeakerID(0),TwitterID(0),SpeakerImage(0),SpeakerTitle(0),SpeakerName(0),TEDxLogo(0)
	{		
		m_elementsMap["Root"]=(IGUIElement**)&Root;
		m_elementsMap["Contents"]=(IGUIElement**)&Contents;
		m_elementsMap["SessionName"]=(IGUIElement**)&SessionName;
		m_elementsMap["SessionStartTime"]=(IGUIElement**)&SessionStartTime;
		m_elementsMap["SessionEndTime"]=(IGUIElement**)&SessionEndTime;
		m_elementsMap["SpeakerID"]=(IGUIElement**)&SpeakerID;
		m_elementsMap["TwitterID"]=(IGUIElement**)&TwitterID;
		m_elementsMap["SpeakerImage"]=(IGUIElement**)&SpeakerImage;
		m_elementsMap["SpeakerTitle"]=(IGUIElement**)&SpeakerTitle;
		m_elementsMap["SpeakerName"]=(IGUIElement**)&SpeakerName;
		m_elementsMap["TEDxLogo"]=(IGUIElement**)&TEDxLogo;

	}

};
}