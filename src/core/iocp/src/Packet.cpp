#include"core/iocp/include/Packet.h"
#include"core/iocp/include/Socket.h"


Packet* _Packet::CreatePacket(Socket*  socketCtx, size_t _maxBufSize)
{
	Packet* packet = (Packet*)MALLOC(sizeof(Packet));
	memset(packet, 0, sizeof(Packet));

	packet->socketCtx = socketCtx;
	packet->serverTask = socketCtx->serverTask;
	packet->socketID = socketCtx->GetID();
	packet->postIoType = POST_IO_NULL;

	packet->maxBufSize = _maxBufSize;

	if (_maxBufSize != 0)
	{
		packet->buf = (char*)MALLOC(packet->maxBufSize);
		memset(packet->buf, 0, packet->maxBufSize);
	}

	packet->packBuf.buf = packet->buf;
	packet->packBuf.len = (ULONG)packet->maxBufSize;

	return packet;
}

void _Packet::ResetBuffer(Packet* packet, char* buf, size_t bufsize, bool isCopy)
{
	if (buf == nullptr || bufsize <= 0)
	{
		FREE(packet->buf);
		packet->maxBufSize = 0;
		packet->packBuf.buf = packet->buf;
		packet->packBuf.len = (ULONG)packet->maxBufSize;
		return;
	}

	if (isCopy)
	{
		packet->buf = (char*)MALLOC(bufsize);
		memcpy(packet->buf, buf, bufsize);
	}
	else
	{
		packet->buf = buf;
	}

	packet->maxBufSize = bufsize;
	packet->packBuf.buf = packet->buf;
	packet->packBuf.len = (ULONG)packet->maxBufSize;
}


void _Packet::ClearBuffer(Packet* packet)
{
	memset(packet->buf, 0, packet->maxBufSize);
}


// 释放掉Socket
void _Packet::ReleasePacket(Packet* packet)
{
	FREE(packet->buf);
	FREE(packet);
}
