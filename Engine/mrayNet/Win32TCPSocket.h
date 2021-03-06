

#ifndef ___Win32TCPSocket___
#define ___Win32TCPSocket___




#include "IReliableSocket.h"
#include "SendPacketQueue.h"
#include "NetDataTypes.h"
#include "HNetCommands.h"
#include "IThread.h"
#include "NetAddress.h"
#include "IThreadFunction.h"

#include "CompileConfig.h"

#include "Win32Socket.h"
#include <winsock2.h>
#include <GCPtr.h>

namespace mray{
namespace network{

class Win32TCPSocketProcessThread;

class Win32TCPSocket:public IReliableSocket
{
protected:
	friend class Win32TCPSocketProcessThread;
	SOCKET m_socket;
	Win32Socket* m_attachedSock;

	struct STCPPeer{
		SOCKET m_socket;
		NetAddress address;
	};
	
	std::list<STCPPeer*> m_connectedPeers;
	std::list<SPacket*>	m_graveyardPackets;
	std::list<SPacket*>	m_recivedPackets;
	std::list<SPacket*>	m_sendPackets;
	std::list<NetAddress>	m_toClosePeers;

	GCPtr<OS::IMutex>	m_toCloseMutex;
	GCPtr<OS::IMutex>	m_peersMutex;
	GCPtr<OS::IThread> m_thread;
	OS::IThreadFunction* m_threadFunc;

	bool m_isManual;
	
	NetAddress m_localAddress;

	int m_sleepInterval;
	int m_maxConnection;
	
	void deletePeer(STCPPeer*peer);


	SPacket* getOrCreatePacket();
	void sendPacket(void*data,uint size,const NetAddress&addr);

	STCPPeer*getPeer(const NetAddress&addr);

public:
	Win32TCPSocket(Win32Socket*sock);
	virtual~Win32TCPSocket();

	void SetManualSocket(bool m);
	bool startSocket(int port,int maxIncomingConnection,int sleepInterval);
	void stopSocket();

	void closeConnection();

	bool isConnected();

	const NetAddress *connect(const core::string&host,ushort port);
	const NetAddress *connect(const NetAddress&addr);

	virtual uint GetPeerSocket(const NetAddress&addr) ;
	bool peerConnected(const NetAddress&addr);
	
	void GetConnectedPeers(std::list<NetAddress>& peers);

	const NetAddress*getAddress();

	void send(void*data,uint size,bool imediate,const NetAddress&addr);

	virtual int receive(const NetAddress& peer, void* data, uint size, int flags);

	SPacket*popMessage();

	void update();

};

}
}

#endif


