#pragma once

#include"core/iocp/include/ServerBaseTypes.h"
#include"core/iocp/include/DePacketor.h"
#include"core/iocp/include/Packet.h"
#include"utils/include/ByteStream.h"
#include"core/iocp/include/MsgCoroutine.h"

class DLL_CLASS Socket : public CmiNewMemory
{
public:
	// 初始化
	Socket(ServerTask* _serverTaskCtx = NULL);

	// 释放资源
	virtual ~Socket();

	Server* GetServer();

	void SetSocketType(SocketType type)
	{
		socketType = type;
	}

	void SetSocketState(SocketState state);
	void SetRemoteServerInfo(ServerInfo  *serverInfo);

	int Send(Packet* packet, int delay = 0);
	int Send(ByteStream& packetStream, int delay = 0);
	int Send(ByteStream& packetStream, size_t headerlen, int delay = 0);
	int Close(int delay = 0);
	int DirectClose();

	Packet* CreatePacket(int packSize = MAX_BUFFER_LEN);
	Packet* CreatePacket(uint8_t* packBuf, size_t packSize);


	void SetPack(uint8_t* pack, int32_t packlen)
	{
		this->pack = pack;
		packSize = packlen;
	}

	void RemovePack()
	{
		pack = nullptr;
		packSize = 0;
	}


	uint8_t* GetRecvedPackPtr()
	{
		return pack;
	}

	size_t GetRecvedPackSize()
	{
		return packSize;
	}

	void SetDataLengthToPackHead(uint8_t* pack, size_t dataLen)
	{
		dePacketor->SetDataLengthToPackHead(pack, dataLen);
	}


	socket_id_t GetID()
	{
		return id;
	}

	void ResetGenID();

	void SetDePacketor(DePacketor* depacketor);
	void SetMsgProcesser(BaseMsgProcesser* _msgProcesser);

	DePacketor* GetDePacketor()
	{
		return dePacketor;
	}

	BaseMsgProcesser* GetMsgProcesser()
	{
		return msgProcesser;
	}


	SocketType GetSocketType()
	{
		return socketType;
	}

	SocketState GetSocketState()
	{
		return socketState;
	}

	void UpdataTimeStamp(uint64_t _timeStamp)
	{
		timeStamp = _timeStamp;
	}

	void UpdataTimeStamp();


	ServerInfo* GetRemoteServerInfo()
	{
		return remoteServerInfo;
	}

	void ChangeDataTransMode(DataTransMode mode)
	{
		dataTransMode = mode;
	}

private:
	void ReleaseRes();
	void Release();

public:

	SOCKET             sock;

	char               remoteIP[16];             // 远端IP地址
	USHORT             remotePort;               // 远端连接端口
	DataTransMode      dataTransMode;
	uint64_t           timeStamp;
	PackBuf            unPackCache;
	int32_t            unPackCacheSize;
	int32_t            unPackCalcLen;
	size_t             unPackHeadLen;
	uint8_t*           cachePack;
	int32_t            cachePackLen;
	ExtractState       extractState;
	ByteStream*        joinFrameDataStream;
	void*              joinFrameHeader;

	uint8_t           *pack;
	size_t             packSize;
	ServerTask        *serverTask;

	uint64_t           tag;

private:
	SocketType    socketType;
	SocketState   socketState;
	ServerInfo    *remoteServerInfo;
	
	DePacketor*    dePacketor;
	BaseMsgProcesser* msgProcesser;

	PacketList    *sendList;
	socket_id_t    id;

	friend class ServerTask;
};

