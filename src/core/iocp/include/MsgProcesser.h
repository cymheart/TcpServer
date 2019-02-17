#pragma once
#include"core/iocp/include/ServerBaseTypes.h"
#include"utils/include/ByteStream.h"
#include"core/iocp/include/Socket.h"
#include"core/iocp/include/DePacketor.h"
#include <functional>
using std::function;
using namespace std::placeholders;


#define SOCKET_EV_FUNC_OBJ(_Class, _Func, _Object) std::bind(&_Class::_Func, _Object, _1)
#define MSG_FUNC_OBJ(_Class, _Func, _Object) std::bind(&_Class::_Func, _Object, _1, _2, _3)


#define SOCKET_EV_FUNC(_Class, _Func) std::bind(&_Class::_Func, this, _1)
#define MSG_FUNC(_Class, _Func) std::bind(&_Class::_Func, this, _1, _2, _3)


class BaseMsgProcesser
{
public:
	virtual void SetDePacketor(DePacketor* dePacketor) = 0;
	virtual void UnPack(SocketEvent ev, Socket& socket) = 0;	
	virtual void ReleaseHeader(void* header) = 0;
};

template <class HeaderType, class MsgType>
class MsgProcesser:public BaseMsgProcesser
{
public:
	MsgProcesser()
		:isDecryptData(false)
	{

	}

	typedef function<void(Socket& socket)> SocketEventFunc;
	typedef function<void(Socket& socket, HeaderType& header, ByteStream& dataStream)> MsgFunc;

	void SetDePacketor(DePacketor* dePacketor)
	{
		dePacketor->SetUnPackCallBack(_UnPack, this);
	}

	void SetIsDecryptData(bool isDecrypt)
	{
		isDecryptData = isDecrypt;
	}

	int RegSocketEvent(SocketEvent ev, SocketEventFunc socketEventCB)
	{
		if (socketEventCB) {
			vector<SocketEventFunc>& socketEvCBList = socketEventFuncMap[ev];
			socketEvCBList.push_back(socketEventCB);
			return 0;
		}
		return 1;
	}

	void RegMsg(MsgType msgType, MsgFunc msgCB)
	{
		if (msgCB)
		{
			msgFuncMap[msgType] = msgCB;
		}
	}

	void ProcessMsg(Socket& socket)
	{
		HeaderType header;
		uint8_t* pack = socket.GetRecvedPackPtr();

		ByteStream recvStream((uint8_t*)pack, socket.GetRecvedPackSize());
		int headlen = ReadStreamToHeader(header, recvStream, true);
		recvStream.ResetExtrenBuf(recvStream.GetCurt(), recvStream.GetNumberOfRichBytes());

		if (!IsSingleFrame(header))
		{	
			if (isDecryptData) {
				ByteStream decryptDataStream;
				Decrypt(header, recvStream, decryptDataStream);
				decryptDataStream.SetCurt(0);
				_ProcessMsg(GetMsgTypeValue(header), socket, header, decryptDataStream);
				uint8_t* buf = decryptDataStream.GetExternBuf();
				FREE(buf);
			}
			else
			{
				_ProcessMsg(GetMsgTypeValue(header), socket, header, recvStream);
			}
		}
		else
		{	
			_ProcessSingleFrame(socket, header, recvStream);
		}
	}


	void ProcessSocketEvent(SocketEvent ev, Socket& socket)
	{
		auto iter = socketEventFuncMap.find(ev);
		if (iter != socketEventFuncMap.end())
		{
			bool isContinue = true;
			vector<SocketEventFunc>& socketEvCBList = iter->second;
			for (int i = 0; i < socketEvCBList.size(); i++)
			{
				if (isContinue) {
					(socketEvCBList[i])(socket);
				}
				else {
					break;
				}
			}
		}
	}


protected:

	//判断此包是否为分片
	virtual bool IsSingleFrame(HeaderType& frameHeader)
	{
		return false;
	}

	//获取当前包消息类型
	virtual MsgType GetMsgTypeValue(HeaderType& header) = 0;

	//复制头部结构数据
	virtual HeaderType* CopyHeader(HeaderType& header) = 0;

	//删除头部结构数据
	virtual void ReleaseHeader(HeaderType* header) = 0;

	//读取数据流中的头部数据到头部结构中
	virtual int ReadStreamToHeader(HeaderType& header, ByteStream& readStream, bool isNetToHost) = 0;

	//处理分片包加入到总包中
	virtual bool ProcessSingleDataFrame(
		HeaderType& joinFrameHeader, ByteStream& joinFrameDataStream,
		HeaderType& singleFrameHeader, ByteStream& singleFrameDataStream) = 0;

	//解密数据
	virtual int Decrypt(HeaderType& header, ByteStream& orgDataStream, ByteStream& decryptDataStream)
	{
		return 0;
	}


private:

	void ReleaseHeader(void* header)
	{
		ReleaseHeader((HeaderType*)header);
	}

	void _ProcessSingleFrame(Socket& socket, HeaderType& singleFrameHeader, ByteStream singleFrameDataStream)
	{
		if (socket.joinFrameDataStream == nullptr) {
			socket.joinFrameDataStream = new ByteStream;
			socket.joinFrameDataStream->OpenByteAlign(false);
			socket.joinFrameHeader = (void*)CopyHeader(singleFrameHeader);
			socket.joinFrameDataStream->WriteBytes(singleFrameDataStream.GetBuf(), singleFrameDataStream.GetNumberOfWriteBytes());
			return;
		}

		bool isFinish = ProcessSingleDataFrame(
			*(HeaderType*)socket.joinFrameHeader, *socket.joinFrameDataStream,
			singleFrameHeader, singleFrameDataStream);

		if (isFinish)
		{
			HeaderType& header = *(HeaderType*)socket.joinFrameHeader;
			socket.joinFrameDataStream->SetCurt(0);

			if (isDecryptData) {
				ByteStream decryptDataStream;
				Decrypt(header, *socket.joinFrameDataStream, decryptDataStream);
				decryptDataStream.SetCurt(0);
				_ProcessMsg(GetMsgTypeValue(header), socket, header, decryptDataStream);
				uint8_t* buf = decryptDataStream.GetExternBuf();
				FREE(buf);
			}
			else
			{
				_ProcessMsg(GetMsgTypeValue(header), socket, header, *socket.joinFrameDataStream);
			}

			ReleaseHeader(&header);
			RELEASE(socket.joinFrameDataStream);
			socket.joinFrameHeader = nullptr;
			socket.joinFrameDataStream = nullptr;
		}
	}

	void _ProcessMsg(MsgType msgType, Socket& socket, HeaderType& header, ByteStream& dataStream)
	{
		auto iter = msgFuncMap.find(msgType);
		if (iter != msgFuncMap.end())
		{
			(iter->second)(socket, header, dataStream);
		}
		else
		{
			WARNING("消息(%d)没有对应处理函数!", msgType);
		}
	}

	void UnPack(SocketEvent ev, Socket& socket)
	{
		switch (ev)
		{
		case EV_SOCKET_RECV:
		{
			if (socket.dataTransMode == MODE_PACK)
				ProcessMsg(socket);
			else
				ProcessSocketEvent(ev, socket);
		}
		break;

		default:
			ProcessSocketEvent(ev, socket);
			break;
		}
	}

	static void _UnPack(SocketEvent ev, Socket& socket, void* param)
	{
		MsgProcesser<HeaderType, MsgType>* msgProcesser = (MsgProcesser<HeaderType, MsgType>*)param;
		socket.SetMsgProcesser(msgProcesser);
		msgProcesser->UnPack(ev, socket);
	}

private:

	typedef unordered_map<MsgType, MsgFunc, hash<MsgType>, equal_to<MsgType>, CmiAlloc<pair<MsgType, MsgFunc>>> MsgFuncMap;
	typedef unordered_map<SocketEvent, vector<SocketEventFunc>, hash<SocketEvent>, equal_to<SocketEvent>, CmiAlloc<pair<SocketEvent, vector<SocketEventFunc>>>> SocketEventFuncMap;

	MsgFuncMap         msgFuncMap;
	SocketEventFuncMap socketEventFuncMap;

	bool isDecryptData;
};
