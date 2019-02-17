#pragma once
#include"core/iocp/include/ServerBaseTypes.h"
#include <functional>
using std::function;
using namespace std::placeholders;


#define MSG_COROUT_FUNC(_Class, _Func) std::bind(&_Class::_Func, this, _1)


class BaseMsgCoroutine:public CmiNewMemory
{
public:
	virtual ~BaseMsgCoroutine()
	{

	}

	virtual LPVOID GetHandle() = 0;
	virtual void SwitchToOther(LPVOID coHandle) = 0;
	virtual void SwitchToMySelf() = 0;

	static LPVOID GetCurrentCoHandle()
	{
		return GetCurrentFiber();
	}

	static void CoYield(LPVOID switchCoHandle)
	{
		SwitchToFiber(switchCoHandle);
	}
};


template <class HeaderType>
class MsgCoroutine:public BaseMsgCoroutine
{
public:
	typedef function<void(MsgCoroutine& co)> MsgCoroutineFunc;

public:
	MsgCoroutine(Server* ser, MsgCoroutineFunc coFunc)
		:server(ser)
		,handle(nullptr)
		,coFunc(nullptr)
		,socket(nullptr)
		,header(nullptr)
		,dataStream(nullptr)
	{
		this->coFunc = coFunc;
		handle = CreateFiberEx(1024 * 200, 0, FIBER_FLAG_FLOAT_SWITCH, coroutine_func, this);	
		server->SaveMsgCoroutine(this);
	}

	~MsgCoroutine()
	{
		DeleteFiber(handle);
	}

	void SetCurtParams(Socket& socket, HeaderType& header, ByteStream& dataStream)
	{
		this->socket = &socket;
		this->header = &header;
		this->dataStream = &dataStream;
	}

	void SetCurtParams(Socket* socket, HeaderType* header, ByteStream* dataStream)
	{
		this->socket = socket;
		this->header = header;
		this->dataStream = dataStream;
	}

	void SetCurtParams(ByteStream& dataStream)
	{
		this->dataStream = &dataStream;
	}

	void ClearCurtParams()
	{
		socket = nullptr;
		header = nullptr;
		dataStream = nullptr;
	}

	void SwitchToMySelf()
	{
		SwitchToFiber(handle);
	}

	void SwitchToOther(LPVOID coHandle)
	{
		if (handle == coHandle)
			return;
		SwitchToFiber(coHandle);
	}

	LPVOID GetHandle()
	{
		return handle;
	}

	Socket& GetSocket()
	{
		return *socket;
	}

	HeaderType& GetHeader()
	{
		return *header;
	}

	ByteStream& GetDataStream()
	{
		return *dataStream;
	}


private:
	static void __stdcall coroutine_func(LPVOID lpParameter)
	{
		MsgCoroutine* co = (MsgCoroutine*)lpParameter;
		(co->coFunc)(*co);
		SwitchToFiber(co->server->GetMainFiberHandle());
		co->server->DelMsgCoroutine(co);	
	}

private:
	Server* server;
	MsgCoroutineFunc coFunc;
	LPVOID handle;

	Socket* socket;
	HeaderType* header;
	ByteStream* dataStream;


};