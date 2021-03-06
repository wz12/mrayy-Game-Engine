
#include "stdafx.h"
#include "SessionRenderer.h"

#include "SessionContainer.h"
#include "CSpeaker.h"
#include "SessionDetails.h"

#include "SpeakerNode.h"
#include "TweetNode.h"
#include "IThreadManager.h"
#include "NodeRenderer.h"
#include "TwitterTweet.h"
#include "SceneCamera.h"
#include "AppData.h"
#include "ViewPort.h"

namespace mray
{
namespace scene
{


SessionRenderer::SessionRenderer()
{
	m_sessions = 0;
	m_activeSpeaker = 0;
	{
		m_physics = new msa::physics::World2D();
		m_physics->setGravity(0);
	}
	m_hoverItem = 0;
	m_nodeRenderer = new NodeRenderer();
	m_dataMutex = OS::IThreadManager::getInstance().createMutex();
	m_camera = new SceneCamera;

	m_speakerDistance = 400;
	m_TweetsDistance = 80;
	gAppData.SpeakerChange.AddListener(this);
}

SessionRenderer::~SessionRenderer()
{
	delete m_physics;
	delete m_dataMutex;
	delete m_nodeRenderer;
	delete m_camera;
}

void SessionRenderer::OnPointerMoved(const math::vector2d& pos)
{
}



void SessionRenderer::SetRenderingVP(const math::rectf& vp)
{
	m_camera->SetViewPort(vp);
}

void SessionRenderer::SetSessions(ted::SessionContainer*sessions)
{
	m_sessions = sessions;

	if (!m_sessions)
		return;

	const std::vector<ted::SessionDetails*>& slist=m_sessions->GetSessions();

	int speakerCount = 0;
	for (int i = 0; i < slist.size(); ++i)
	{
		speakerCount += slist[i]->GetSpeakers().size();
	}
	std::vector<SpeakerNode*> spList;
	float rad = 400;
	math::vector2d pos;
	for (int i = 0; i < slist.size();++i)
	{
		const std::vector<ted::CSpeaker*>& speakers = slist[i]->GetSpeakers();
		for (int j = 0; j < speakers.size(); ++j)
		{
			SpeakerNode* s = new SpeakerNode(speakers[j]);



			msa::physics::Particle2D* n = new msa::physics::Particle2D(pos);
			m_physics->addParticle(n);
			n->makeFixed();
			//msa::physics::Spring2D* spr = m_physics->makeSpring(root, n,  0.005, rad);

			s->SetSize(40);
			n->setRadius(s->GetSize() );
			
			s->SetPhysics(n);
			m_speakers[s->GetUserDisplyName()] = s;
			m_speakersSeq.push_back(s);

			spList.push_back(s);
			m_renderNodes.push_back(s);

			pos.y += m_speakerDistance;
			pos.x = math::Randomizer::randRange(-200, 200);
		}
	}

}

void SessionRenderer::AddTweets(const std::vector<ted::TwitterTweet*> &Tweets)
{
	for (int i = 0; i < Tweets.size(); ++i)
	{
		bool found = false;
		if (Tweets[i]->replyToTweet != 0)
		{
			TweetMap::iterator it = m_Tweets.find(Tweets[i]->replyToTweet->ID);
			if (it != m_Tweets.end())
			{

				for (int j = 0; j < m_speakersSeq.size(); ++j)
				{
					if (it->second->GetSpeaker() == m_speakersSeq[j]->GetSpeaker())
					{
						_AddTweetNode(Tweets[i], m_speakersSeq[j]);
						found = true;
						break;;
					}
				}
			}

		}
		else
		{
			core::stringw txt = Tweets[i]->text;
			txt.make_lower();

			for (int j = 0; j < m_speakersSeq.size(); ++j)
			{
				//if (Tweets[i]->HasHashtag(m_speakersSeq[j]->GetSpeaker()->GetTwitterID()))
				core::stringw id = core::string_to_wchar(  m_speakersSeq[j]->GetSpeaker()->GetTwitterID());
				id.make_lower();
				if (txt.find(id)!=-1)
				{
					_AddTweetNode(Tweets[i], m_speakersSeq[j]);
					found = true;
					break;
				}
			}
		}
// 		if (!found)
// 			delete Tweets[i];
	}
}

void SessionRenderer::_AddTweetNode(ted::TwitterTweet* t, SpeakerNode*speaker)
{
	m_dataMutex->lock();
	if (m_Tweets.find(t->ID) != m_Tweets.end())
	{
		m_dataMutex->unlock();
		return;
	}
	TweetNode* node = new TweetNode(speaker->GetSpeaker(),t);

	m_Tweets[node->GetTweetID()] = node;

	float sz = 25;
	msa::physics::Particle2D* n = new msa::physics::Particle2D();
	n->setRadius(sz);
	m_physics->addParticle(n);
	node->SetPhysics(n);


	ted::TwitterTweet* replyTweet = node->GetTweet()->replyToTweet;
	ITedNode* target = 0;
	if (replyTweet)
	{
		TweetMap::iterator it = m_Tweets.find(replyTweet->ID);
		if (it != m_Tweets.end())
		{
			it->second->AddTweet(node);
			target = it->second;
		}
	}
	if (!target)
	{
		speaker->AddTweet(node);
		target = speaker;
	}

	msa::physics::Particle2D *ph = target->GetPhysics();
	math::vector2d pos = ph->getPosition();
	float a = math::Randomizer::rand01() * 360;
	msa::physics::Particle2D* nph = node->GetPhysics();
	float r = math::Randomizer::rand01() * 50 + 50 + nph->getRadius() + ph->getRadius();
	float r2 = r + 300;
	pos.x += math::cosd(a) * r2;
	pos.y += math::sind(a) * r2;
	nph->moveTo(pos, true);
	msa::physics::Spring2D* spr = m_physics->makeSpring(ph, nph, 0.0001, r);


	m_dataMutex->unlock();
}

void SessionRenderer::_AddTweetsNodes(const std::vector<TweetNode*> &nodes)
{
	if (!nodes.size())
		return;
	gAppData.soundManager->playSound("sounds//TweetArrived.mp3", 0, true, 100, false, sound::ESNDT_2D);
	m_dataMutex->lock();
	for (int i = 0; i < nodes.size(); ++i)
	{
		
		m_Tweets[nodes[i]->GetTweetID()] = nodes[i];

		float sz = 25;
		msa::physics::Particle2D* n = new msa::physics::Particle2D();
		n->setRadius(sz );
		m_physics->addParticle(n);
		nodes[i]->SetPhysics(n);
		//m_speakers.find(nodes[i]->GetSpeakerID());
	}

	for (int i = 0; i < nodes.size(); ++i)
	{
		 ted::TwitterTweet* t= nodes[i]->GetTweet()->replyToTweet;
		 ITedNode* target = 0;
		 if (t)
		 {
			 TweetMap::iterator it= m_Tweets.find(t->ID);
			 if (it != m_Tweets.end())
			 {
				 it->second->AddTweet(nodes[i]);
				 target = it->second;
			 }
		 }
		 if (!target)
		{
			int s = math::Randomizer::rand(m_speakers.size());
			SpeakerMap::iterator  it = m_speakers.begin();
			std::advance(it, s);
			it->second->AddTweet(nodes[i]);
			target = it->second;
		}

		 msa::physics::Particle2D *ph = target->GetPhysics();
		 math::vector2d pos = ph->getPosition();
		 float a = math::Randomizer::rand01() * 360;
		 msa::physics::Particle2D* nph= nodes[i]->GetPhysics();
		 float r = math::Randomizer::rand01() * 50 + 50 + nph->getRadius() + ph->getRadius();
		 float r2 = r + 300;
		 pos.x += math::cosd(a) * r2;
		 pos.y += math::sind(a) * r2;
		 nph->moveTo(pos, true);
		 msa::physics::Spring2D* spr = m_physics->makeSpring(ph, nph, 0.0001, r );


	}
	//m_renderNodes.insert(m_renderNodes.end(), nodes.begin(), nodes.end());
	m_dataMutex->unlock();
}

void SessionRenderer::_OnSpeakerChange(ted::CSpeaker*s)
{
	SpeakerMap::iterator it= m_speakers.find(s->GetTwitterID());
	if (it == m_speakers.end())
		return;
	m_activeSpeaker = it->second;
	SetSelectedItem(m_activeSpeaker);
}


ITedNode* SessionRenderer::GetNodeFromPosition(const math::vector2d& pos)
{
	math::vector2d p = m_camera->ConvertToWorldSpace(pos);
	m_dataMutex->lock();
	ITedNode* ret = 0;
	SpeakerMap::iterator  it = m_speakers.begin();

	for (; it != m_speakers.end();++it)
	{
		ret= it->second->GetNodeFromPoint(p);
		if (ret)
			break;;
	}
	m_dataMutex->unlock();
	return ret;
}


void SessionRenderer::Update(float dt)
{
	m_dataMutex->lock();
	m_physics->update(1);
	SpeakerMap::iterator  it = m_speakers.begin();

	for (; it != m_speakers.end(); ++it)
	{
		it->second->Update(dt);
	}
	m_dataMutex->unlock();
	//m_translation.x += 20 * dt;
	//SetTransformation(m_translation, 0, 1);

	if (m_selectedItem)
		m_camera->FrameBox(m_selectedItem->GetBoundingBox(true));
	else
		m_camera->FrameBox(CalcAllBox());
	m_camera->Update(dt);
}
void SessionRenderer::Draw()
{
	video::IVideoDevice* dev = Engine::getInstance().getDevice();

	math::matrix4x4 oldT;
	dev->getTransformationState(video::TS_WORLD,oldT);
	m_camera->ApplyTransformation();
	m_dataMutex->lock();
	m_nodeRenderer->Clear();
	m_nodeRenderer->SetClippingVP(m_camera->GetWorldSpaceViewPort());
	SpeakerMap::iterator  it, it2;
	for (int i = 0; i<m_speakersSeq.size()-1; ++i)
	{
		m_nodeRenderer->AddSpeakerSpeaker(m_speakersSeq[i], m_speakersSeq[i+1]);
	}
	it = m_speakers.begin();
	for (; it != m_speakers.end(); ++it)
	{
		it->second->Draw(m_nodeRenderer,m_camera->GetWorldSpaceViewPort());
	}
	m_dataMutex->unlock();

	if (gAppData.Debugging)
	{
		dev->unuseShader();
		if (m_selectedItem)
		{
			dev->draw2DRectangle(m_selectedItem->GetBoundingBox(true), 1, false);
		}
	}
	m_nodeRenderer->RenderAll(this);
	dev->setTransformationState(video::TS_WORLD, oldT);
}

math::rectf SessionRenderer::CalcAllBox()
{
	math::rectf rc;
	SpeakerMap::iterator  it = m_speakers.begin();
	for (; it != m_speakers.end(); ++it)
	{
		math::rectf r = it->second->GetBoundingBox(true);
		if (it == m_speakers.begin())
			rc = r;
		else
		{
			rc.addPoint(r.ULPoint);
			rc.addPoint(r.BRPoint);
		}
	}
	return rc;
}

void SessionRenderer::SetHoverdItem(ITedNode* node)
{
	if (m_hoverItem)
		m_hoverItem->OnHoverOff();
	m_hoverItem = node;
	if (m_hoverItem)
		m_hoverItem->OnHoverOn();

}

void SessionRenderer::SetSelectedItem(ITedNode* node)
{
	if (m_selectedItem)
		m_selectedItem->OnSelectedOff();
	m_selectedItem = node;
	if (!m_selectedItem)
		m_selectedItem = m_activeSpeaker;
	if (m_selectedItem)
		m_selectedItem->OnSelectedOn();

}

}
}
