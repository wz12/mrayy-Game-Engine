

/********************************************************************
	created:	2014/03/31
	created:	31:3:2014   11:32
	filename: 	C:\Development\mrayEngine\Projects\TEDxTokyo\TwitterVisualizer\TwitterTweet.h
	file path:	C:\Development\mrayEngine\Projects\TEDxTokyo\TwitterVisualizer
	file base:	TwitterTweet
	file ext:	h
	author:		MHD Yamen Saraiji
	
	purpose:	
*********************************************************************/

#ifndef __TwitterTweet__
#define __TwitterTweet__


#include "DataTypes.h"
#include "DateTime.h"

class SACommand;
namespace mray
{
namespace ted
{
	class TwitterUserProfile;

class TwitterTweet
{
protected:
public:
	static std::map<IDType, uint> TweetIDMap;
	static std::vector<TwitterTweet*> TwitterTweetList;
	static void AddTwitterTweet(TwitterTweet* t);
	static TwitterTweet* GetTweetByID(IDType id);

public:
	TwitterTweet() :ID(0), user(0), retweets(0)
	{}
	virtual~TwitterTweet(){}

	IDType ID;
	TwitterUserProfile* user;
	core::stringw text;
	core::CDate date;
	int retweets;

	struct Entities
	{
		std::vector<core::stringw> hashTags;
		std::vector<core::string> urls;
		std::vector<TwitterUserProfile*> user_mentions;
	}entities;

	void LoadXML(xml::XMLElement* e);
	void SaveXML(xml::XMLElement* e);
};

}
}


#endif