#pragma once

#include"core/iocp/include/ServerBaseTypes.h"
#include"utils/include/ByteStream.h"

class DLL_CLASS DePacketor : public CmiNewMemoryRef
{
public:
	DePacketor();
	~DePacketor();
	
	void SetMsgProcesser(BaseMsgProcesser* _msgProcesser);

	void SetUnPackCallBack(UnPackCallBack _unPackCallBack, void* param = NULL)
	{
		unPack = _unPackCallBack;
		unPackParam = param;
	}

	void SetGetPackDataLengthCallBack(GetPackDataLenCB _getPackDataLength)
	{
		getPackDataLength = _getPackDataLength;
	}

	void SetSetDataLengthToPackHeadCallBack(SetDataLengthToPackHeadCallBack _setDataLengthToPackHead)
	{
		setDataLengthToPackHead = _setDataLengthToPackHead;
	}

	
	//获取包头预设长度	
	ULONG GetPackHeadPreLength()
	{
		return packHeaderPreLen;  
	}


	//获取包数据长度
	int32_t GetPackDataLength(uint8_t* pack, size_t packLen, int32_t* realPackHeadLen = nullptr)
	{
		if (getPackDataLength != nullptr) {
			int32_t packHeadLen = -1;
			int32_t datalen = getPackDataLength(this, pack, packLen, &packHeadLen);
			if (realPackHeadLen != nullptr)
				*realPackHeadLen = packHeadLen;
			return datalen;
		}

		if(realPackHeadLen != nullptr)
			*realPackHeadLen = -1;

		return -1;
	}

	//获取包尾长度	
	ULONG GetPackTailLength()
	{
		return packTailSize; //0
	}


	//获取包长度
	int32_t GetPackLength(uint8_t* pack, size_t packLen, int32_t* realPackHeadLen)
	{
		int32_t packHeadLen = 0;
		int32_t dataLen = GetPackDataLength(pack, packLen, &packHeadLen);
		if (dataLen == -1 || packHeadLen == -1)
			return -1;

		if (realPackHeadLen != nullptr)
			*realPackHeadLen = packHeadLen;

		int32_t size = packHeadLen + dataLen + GetPackTailLength();
		return size;
	}

	//设置包头预设长度	
	void SetPackHeadPreLength(ULONG sz)
	{
		packHeaderPreLen = sz;
	}

	//设置包尾长度	
	void SetPackTailLength(ULONG sz)
	{
		packTailSize = sz;
	}


	//设置数据尺寸到包头中保存
	void SetDataLengthToPackHead(uint8_t* pack, size_t dataSize)
	{
		if (setDataLengthToPackHead != NULL) {
			setDataLengthToPackHead(pack, dataSize);
			return;
		}
	}

	size_t GetMaxBufferSize()
	{
		return maxBufferSize;
	}

	void SetName(CmiString& _name)
	{
		name.assign(_name);
	}

	void SetName(char* _name)
	{
		name.assign(_name);
	}

	CmiString& GetName()
	{
		return name;
	}

private:
	int Extract();
	void SetCurtPack(Socket* _curtPackSocketCtx, uint8_t* _curtPack, size_t _curtPackLen);
	void CreateCachePackBuf();
	void ReleaseCachePackBuf();

	//从对方socket那边接收到的数据包在这个函数中处理，系统自动调用此函数解包
	void UnPack(SocketEvent ev, Socket& socketCtx);


private:
	CmiString      name;

	uint8_t        *buf;
	Socket*         curtDepacketSocket;     //当前接收包SocketCtx
	uint8_t*        curtPack;              //当前接收包 
	size_t          curtPackLen;           //当前接收包的整个数据长度
	uint8_t*        cachePack;
	size_t          cachePackLen;
	size_t          maxBufferSize;
	int             packHeaderPreLen;
	int             packTailSize;

	UnPackCallBack               unPack;
	void*                        unPackParam;

	GetPackDataLenCB              getPackDataLength;
	SetDataLengthToPackHeadCallBack setDataLengthToPackHead;
	BaseMsgProcesser* msgProcesser;

	friend class ServerTask;
};
