#pragma once

#include"core/iocp/include/ServerBaseTypes.h"


typedef struct DLL_CLASS _Packet
{
	OVERLAPPED      overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)
	size_t          transferedBytes;                          // 传输的字节数
	PostIoType      postIoType;                               // 标识网络操作的类型(对应上面的枚举)
	PackBuf         packBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
	char*           buf;                                       // 这个是PackBuf里具体存字符的缓冲区
	size_t          maxBufSize;
	Socket*         socketCtx;
	uint64_t        socketID;
	ServerTask*     serverTask;

	static Packet* CreatePacket(Socket*  _socketCtx, size_t _maxBufSize = MAX_BUFFER_LEN);
	static void ResetBuffer(Packet* packet, char* buf, size_t bufsize, bool isCopy = false);
	static void ClearBuffer(Packet* packet);
	static void ReleasePacket(Packet* packet);

}Packet;

