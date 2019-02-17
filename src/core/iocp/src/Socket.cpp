#include"core/iocp/include/ServerBaseTypes.h"
#include"core/iocp/include/Socket.h"
#include"core/iocp/include/Server.h"
#include"core/iocp/include/ServerTask.h"
#include"core/iocp/include/MsgProcesser.h"

Socket::Socket(ServerTask* _serverTaskCtx)
:sock(INVALID_SOCKET)
, socketType(UNKNOWN_SOCKET)
, socketState(NEW_CREATE)
, timeStamp(0)
, remoteServerInfo(0)
, dataTransMode(MODE_PACK)
, serverTask(_serverTaskCtx)
, remotePort(0)
, dePacketor(nullptr)
, msgProcesser(nullptr)
, sendList(0)
, pack(0)
, packSize(0)
, tag(0)
, extractState(ES_PACKET_HEADLEN_NOT_GET)
, unPackCalcLen(-1)
, unPackHeadLen(0)
, unPackCacheSize(0)
, joinFrameDataStream(nullptr)
, joinFrameHeader(nullptr)
{
	unPackCache.buf = 0;
	unPackCache.len = 0;

	memset(&remoteIP, 0, sizeof(16));

	if (serverTask == nullptr)
		return;

	Server* server = serverTask->GetServer();
	SetDePacketor(server->dePacketor);

	unPackHeadLen = dePacketor->GetPackHeadPreLength();
	sendList = new PacketList;

	UniqueID& uniqueID = UniqueID::GetInstance();
	if (server->serverTaskCount > 1)		
		id = uniqueID.gen_multi();
	else
		id = uniqueID.gen();
}

// 释放资源
Socket:: ~Socket()
{
	Release();
}

void Socket::ResetGenID()
{
	UniqueID& uniqueID = UniqueID::GetInstance();
	Server* server = GetServer();

	if (server->serverTaskCount > 1)
		id = uniqueID.gen_multi();
	else
		id = uniqueID.gen();
}

void Socket::SetSocketState(SocketState state)
{
	socketState = state;

	if ((socketState == CONNECTED_SERVER ||
		socketState == CONNECTTING_SERVER )&&
		remoteServerInfo != nullptr && 
		remoteServerInfo->dePacketor != nullptr)
	{
		SetDePacketor(remoteServerInfo->dePacketor);
	}
	else
	{
		Server* server = serverTask->GetServer();
		SetDePacketor(server->dePacketor);
	}

	unPackHeadLen = dePacketor->GetPackHeadPreLength();
}


void Socket::SetRemoteServerInfo(ServerInfo  *serverInfo)
{
	remoteServerInfo = serverInfo;
	SetSocketState(socketState);
}

void Socket::SetDePacketor(DePacketor* depacketor)
{
	if (depacketor == dePacketor)
		return;

	if(dePacketor != nullptr)	
		DEBUG("%s:%I64u,减少引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef() - 1);

	RELEASE_REF(dePacketor);
	this->dePacketor = depacketor;

	if (dePacketor != nullptr) {
		dePacketor->addRef();
		DEBUG("%s:%I64u,增加引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef());
		unPackHeadLen = dePacketor->GetPackHeadPreLength();
	}

	RemovePack();

	unPackCache.buf = nullptr;
	unPackCache.len = 0;
	unPackCalcLen = 0;
	cachePack = nullptr;
	cachePackLen = 0;
}

void Socket::SetMsgProcesser(BaseMsgProcesser* _msgProcesser)
{
	if (_msgProcesser == msgProcesser)
		return;

	RELEASE(joinFrameDataStream);

	if (joinFrameHeader != nullptr && msgProcesser != nullptr)
		msgProcesser->ReleaseHeader(joinFrameHeader);

	joinFrameHeader = nullptr;
	msgProcesser = _msgProcesser;
}


void Socket::UpdataTimeStamp()
{
	timeStamp = CmiGetTickCount64();
}

void Socket::ReleaseRes()
{
	RELEASE(remoteServerInfo);

	PacketList::iterator iter;
	if (sendList != NULL)
	{
		for (iter = sendList->begin(); iter != sendList->end();)
		{
			Packet::ReleasePacket(*iter);
			iter = sendList->erase(iter);
		}

		RELEASE(sendList);
	}

	FREE(unPackCache.buf);
	unPackCache.len = 0;

	//
	SetDePacketor(nullptr);
	SetMsgProcesser(nullptr);

}

void Socket::Release()
{
	Server* server = serverTask->GetServer();

	if (server->isReusedSocket)
	{
		lpfnDisConnectEx(sock, NULL, TF_REUSE_SOCKET, 0);
	}
	else
	{
		RELEASE_SOCKET(sock);
	}

	ReleaseRes();
}

Server* Socket::GetServer()
{
	return serverTask->GetServer();
}

int Socket::Send(Packet* packet, int delay)
{
	return serverTask->PostSendTask(packet, delay);
}

int Socket::Send(ByteStream& packetStream, int delay)
{
	uint8_t* pack = (uint8_t*)packetStream.TakeBuf();
	Packet* packet = CreatePacket(0);
	Packet::ResetBuffer(packet, (char*)pack, packetStream.GetNumberOfWriteBytes());

	return serverTask->PostSendTask(packet, delay);
}

int Socket::Send(ByteStream& packetStream, size_t headerlen, int delay)
{	
	uint8_t* pack = (uint8_t*)packetStream.TakeBuf();
	size_t packLen = packetStream.GetNumberOfWriteBytes();
	dePacketor->SetDataLengthToPackHead(pack, packLen - headerlen - dePacketor->GetPackTailLength());

	Packet* packet = CreatePacket(0);
	Packet::ResetBuffer(packet, (char*)pack, packLen);

	return serverTask->PostSendTask(packet, delay);
}

int Socket::Close(int delay)
{
	SetSocketState(NORMAL_CLOSE);
	return serverTask->PostSocketErrorTask(this);
}

int Socket::DirectClose()
{
	SetSocketState(NORMAL_CLOSE);
	return serverTask->SocketError(this);
}

Packet* Socket::CreatePacket(int packSize)
{
	return Packet::CreatePacket(this, packSize);
}

Packet* Socket::CreatePacket(uint8_t* packBuf, size_t packSize)
{
	Packet* packet = CreatePacket(packSize);
	memcpy(packet->buf, packBuf, packSize);
	return packet;
}
