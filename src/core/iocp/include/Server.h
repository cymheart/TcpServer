#pragma once

#include"core/iocp/include/ServerBaseTypes.h"
#include"core/iocp/include/Socket.h"
#include"core/iocp/include/ServerInfo.h"
#include"utils/include/TaskProcesser.h"
#include"core/iocp/include/ServerTaskMgr.h"
#include"utils/include/Timer.h"

class DLL_CLASS Server :public CmiNewMemory
{
public:
	Server();
	virtual ~Server(void);

	// 启动服务器
	virtual bool Start();

	//	停止服务器
	virtual void Stop();

	void SetUseSingleSendTaskProcesser(bool isUse);

	void SetServerMachineID(int machineID);

	int StartListener();
	int ConnectServer(ServerInfo* serverInfo, int delay = 0);

	int Server::PostTask(TaskCallBack processDataCallBack, void* taskData, int delay = 0)
	{
		return serverTaskMgr->PostTaskToServerTaskLine(processDataCallBack, taskData, delay);
	}

	int Server::PostTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return serverTaskMgr->PostTaskToServerTaskLine(processDataCallBack, releaseDataCallBack, taskData, delay);
	}

	void SetServerTaskProcess(int serverTaskIdx, TaskProcesser* taskProcesser);

	Timer* CreateTimer(TimerCallBack timerCB, void* param, int durationMS)
	{
		return CreateTimer(0, timerCB,  param, durationMS);
	}

	Timer* CreateTimer(int serverTaskIdx, TimerCallBack timerCB, void* param, int durationMS);


	void SetCheckHeartBeat(bool bCheck)
	{
		if (isStop)
			isCheckHeartBeat = bCheck;
	}

	// 设置监听端口
	void SetListenPort(int nPort)
	{
		if (!isStop)
			return;

		localListenPort = nPort;
	}

	void SetDePacketor(DePacketor* _dePacketor);
	void SetUnPackCallBack(UnPackCallBack _unPackCallBack, void* param = NULL);


	char* GetLocalIP()
	{
		return localIP;
	}

	DePacketor* GetDePacketor()
	{
		return dePacketor;
	}

	
	Socket* GetSocket(uint64_t socketID, int searchInServerTaskIdx = 0);
	int GetTotalSocketCount();

	ServerTaskMgr* GetServerTaskMgr()
	{
		return serverTaskMgr;
	}

	void SetUseCoroutine(int serverTaskIdx, bool isUse);
	LPVOID GetMainFiberHandle(int serverTaskIdx = 0);

	void SetUseCoroutine(bool isUse)
	{
		SetUseCoroutine(0, isUse);
	}

	void SaveMsgCoroutine(BaseMsgCoroutine* co);
	void DelMsgCoroutine(BaseMsgCoroutine* co);
	BaseMsgCoroutine* GetMsgCoroutine(LPVOID coHandle);
	void MsgCoroutineYield();

public:
		// 加载Socket库
		bool LoadSocketLib();

		// 卸载Socket库
		void UnloadSocketLib()
		{
			WSACleanup();
		}

protected:

	bool IocpPostConnect(Packet* packet);
	bool IocpPostRecv(Packet* packet);
	bool IocpPostAccept(Packet* packet);
	bool IocpPostSend(Packet* packet);

	void DoAccept(Packet* packet);
	bool AssociateSocketContext(Socket *socketCtx);

	// 初始化IOCP
	bool InitializeIOCP();

	
	// 线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI IoThread(LPVOID param);


protected:
	int                          localListenPort;            // 本机监听端口

	bool                         useDefTaskProcesser;
	bool                         isCheckHeartBeat;
	int                          childLifeDelayTime;         // 每次检查所有客户端心跳状态的时间间隔
	bool                         isStop;
	bool                         isReusedSocket;
	int                          serverTaskCount;
	bool                         useSingleSendTaskProcesser;

private:

	ServerTaskMgr*               serverTaskMgr;
	DePacketor*                  dePacketor;

	int                          serverType;
	int		                     nThreads;                   // 生成的线程数量
	char                         localIP[16];                    // 本机IP地址	
	Socket*                      listenCtx;                  // 用于监听的Socket的Context信息
	volatile long                exitIoThreads;

	HANDLE                       iocp;                       // 完成端口的句柄
	HANDLE*                      ioThreads;

	MsgCoroutineHashMap          msgCoroutineMap;
	
	friend class DePacketor;
	friend class Socket;
	friend class ServerTask;
	friend class ServerTaskMgr;

};
