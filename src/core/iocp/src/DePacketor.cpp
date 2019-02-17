#include"core/iocp/include/DePacketor.h"
#include"core/iocp/include/Server.h"
#include"core/iocp/include/ServerTask.h"
#include"core/iocp/include/MsgProcesser.h"

DePacketor::DePacketor()
	:buf(0)
	, packHeaderPreLen(0)
	, packTailSize(0)
	, maxBufferSize(2048)
	, setDataLengthToPackHead(nullptr)
	, unPack(nullptr)
	, unPackParam(nullptr)
	, getPackDataLength(nullptr)
	, curtDepacketSocket(nullptr)
    , curtPack(nullptr)        
	, curtPackLen(0)       
	, cachePack(nullptr)
	, cachePackLen(0)
	, msgProcesser(nullptr)
{

}

DePacketor::~DePacketor()
{
	FREE(buf);
}

void DePacketor::SetMsgProcesser(BaseMsgProcesser* _msgProcesser)
{
	if (_msgProcesser == msgProcesser)
		return;

	msgProcesser = _msgProcesser;
	if (msgProcesser != nullptr) 
		msgProcesser->SetDePacketor(this);
}

void DePacketor::UnPack(SocketEvent ev, Socket& socketCtx)
{
	BaseMsgProcesser* msgProcesser = socketCtx.GetMsgProcesser();

	if(msgProcesser != nullptr)	
		msgProcesser->UnPack(ev, socketCtx);
	else if (unPack != nullptr)	
		unPack(ev, socketCtx, unPackParam);
}

void DePacketor::SetCurtPack(Socket* socket, uint8_t* _curtPack, size_t _curtPackLen)
{
	curtDepacketSocket = socket;
	curtPack = _curtPack;
	curtPackLen = _curtPackLen;
	cachePack = curtDepacketSocket->cachePack;
	cachePackLen = curtDepacketSocket->cachePackLen;
}

void DePacketor::CreateCachePackBuf()
{
	PackBuf* cachePackBuf = &(curtDepacketSocket->unPackCache);

	if (cachePackBuf->buf == nullptr)
	{
		curtDepacketSocket->unPackCacheSize = GetMaxBufferSize() * 2;
		cachePackBuf->buf = (char*)MALLOC(curtDepacketSocket->unPackCacheSize);
		cachePackBuf->len = cachePackLen;
		memcpy(cachePackBuf->buf, cachePack, cachePackLen);
		curtDepacketSocket->cachePack = (uint8_t*)cachePackBuf->buf;
		curtDepacketSocket->cachePackLen = cachePackLen;
	}
}

void DePacketor::ReleaseCachePackBuf()
{
	PackBuf* cachePackBuf = &(curtDepacketSocket->unPackCache);
	if (cachePackBuf->buf != nullptr) {
		FREE(cachePackBuf->buf);
		cachePackBuf->buf = 0;
		cachePackBuf->len = 0;
		curtDepacketSocket->unPackCacheSize = 0;
		curtDepacketSocket->cachePack = nullptr;
		curtDepacketSocket->cachePackLen = 0;
		curtDepacketSocket->unPackCalcLen = -1;
		curtDepacketSocket->unPackHeadLen = GetPackHeadPreLength();
	}
}

int DePacketor::Extract()
{
	int calcPackLen = curtDepacketSocket->unPackCalcLen;   //当前接受包的一个完整消息的长度
	ULONG packHeadLen = curtDepacketSocket->unPackHeadLen;
	PackBuf* cachePackBuf = &(curtDepacketSocket->unPackCache);
	uint8_t tmpc;
	int32_t realPackHeadLen;

	if (cachePackBuf->buf != nullptr)
	{
		int richBufLen = curtDepacketSocket->unPackCacheSize - cachePackBuf->len;

		//新来的数据包curtPack的长度curtPackLen, 不大于缓存cachePackBuf中的富余长度richBufLen
		if (curtPackLen <= richBufLen) {
			memcpy(cachePackBuf->buf + cachePackBuf->len, curtPack, curtPackLen);
			cachePackBuf->len += curtPackLen;
		}
		else
		{
			uint8_t* delBuf = nullptr;

			if (curtDepacketSocket->cachePackLen + curtPackLen > curtDepacketSocket->unPackCacheSize)
			{
				curtDepacketSocket->unPackCacheSize *= 2;
				delBuf = (uint8_t*)cachePackBuf->buf;
				cachePackBuf->buf = (char*)MALLOC(curtDepacketSocket->unPackCacheSize);
			}		
			else if (curtDepacketSocket->cachePackLen > curtDepacketSocket->cachePack - (uint8_t*)cachePackBuf->buf)
			{
				//curtDepacketSocket->cachePack是在cachePackBuf->buf中的指针位置
				delBuf = (uint8_t*)cachePackBuf->buf;
				cachePackBuf->buf = (char*)MALLOC(curtDepacketSocket->unPackCacheSize);
			}

			cachePackBuf->len = curtDepacketSocket->cachePackLen;
			memcpy(cachePackBuf->buf, curtDepacketSocket->cachePack, curtDepacketSocket->cachePackLen);
			memcpy(cachePackBuf->buf + cachePackBuf->len, curtPack, curtPackLen);
			cachePackBuf->len += curtPackLen;

			curtDepacketSocket->cachePack = (uint8_t*)cachePackBuf->buf;
			curtDepacketSocket->cachePackLen = cachePackBuf->len;

			FREE(delBuf);
		}

		cachePack = (uint8_t*)cachePackBuf->buf;
		cachePackLen = cachePackBuf->len;
	}
	else {
		cachePack = curtPack;
		cachePackLen = curtPackLen;
	}
	
	while (1)
	{
		switch (curtDepacketSocket->extractState)	
		{
		case ES_PACKET_HEADLEN_NOT_GET:
		{
			if (cachePackLen >= packHeadLen) 
			{			
				calcPackLen = GetPackLength(cachePack, cachePackLen, &realPackHeadLen); //根据包头中的信息，获取包数据长度
				curtDepacketSocket->unPackCalcLen = calcPackLen;
				packHeadLen = curtDepacketSocket->unPackHeadLen = realPackHeadLen;

				if (packHeadLen < 0)
				{
					CreateCachePackBuf();
					curtDepacketSocket->extractState = ES_PACKET_HEADLEN_NOT_GET;
					return 1;
				}
				else if (calcPackLen < 0)
				{
					CreateCachePackBuf();
					curtDepacketSocket->extractState = ES_PACKET_HEAD_NOT_FULL;
					return 1;
				}
				else
				{
					curtDepacketSocket->extractState = ES_PACKET_HEAD_FULL;
				}
			}
			else  //此次包头长度不完整
			{
				CreateCachePackBuf();
				curtDepacketSocket->extractState = ES_PACKET_HEADLEN_NOT_GET;
				return 1;
			}
		}
		break;

		case ES_PACKET_HEAD_FULL:
		{
			if (calcPackLen == cachePackLen)   //刚好获取的是一个完整的数据包
			{
				curtDepacketSocket->SetPack(cachePack, cachePackLen);
				UnPack(EV_SOCKET_RECV, *curtDepacketSocket);
				curtDepacketSocket->RemovePack();

				ReleaseCachePackBuf();
				curtDepacketSocket->extractState = ES_PACKET_HEADLEN_NOT_GET;
				return 0;
			}
			else if (calcPackLen < cachePackLen)   //获取的数据包长度大于一个完整数据包的长度
			{
				tmpc = cachePack[calcPackLen];
				cachePack[calcPackLen] = '\0';
				curtDepacketSocket->SetPack(cachePack, calcPackLen);
				
				UnPack(EV_SOCKET_RECV, *curtDepacketSocket);
				
				curtDepacketSocket->RemovePack();
				cachePack[calcPackLen] = tmpc;

				cachePack += calcPackLen;
				cachePackLen -= calcPackLen;
				curtDepacketSocket->cachePack = cachePack;
				curtDepacketSocket->cachePackLen = cachePackLen;
				curtDepacketSocket->extractState = ES_PACKET_HEADLEN_NOT_GET;
				curtDepacketSocket->unPackCalcLen = -1;
				packHeadLen = curtDepacketSocket->unPackHeadLen = GetPackHeadPreLength();

			}
			else   //获取的数据包不完整
			{
				if (calcPackLen > GetMaxBufferSize()) {
					curtDepacketSocket->extractState = ES_PACKET_HEADLEN_NOT_GET;
					return 2;
				}

				CreateCachePackBuf();
				curtDepacketSocket->extractState = ES_PACKET_HEAD_FULL;
				return 1;
			}
		}

		break;


		case ES_PACKET_HEAD_NOT_FULL:
		{
			int leavePackHeadLen = packHeadLen - cachePackLen;

			if (curtPackLen < leavePackHeadLen)   //包头信息依然不足
			{
				curtDepacketSocket->extractState = ES_PACKET_HEAD_NOT_FULL;
				return 1;
			}
			else   //不完整数据包中获取到完整的包头信息了
			{
				curtDepacketSocket->extractState = ES_PACKET_HEAD_FULL;
			}
		}
		
		break;	
     }	
 }	
 return 0;
}
