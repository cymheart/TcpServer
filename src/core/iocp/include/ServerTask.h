#pragma once

#include"core/iocp/include/ServerBaseTypes.h"
#include"core/iocp/include/Socket.h"
#include"core/iocp/include/ServerInfo.h"
#include"utils/include/TaskProcesser.h"

class ServerTask : public CmiNewMemory
{
public:
	ServerTask(ServerTaskMgr* serverTaskMgr);
	~ServerTask(void);

public:

	void Start();		
	void Stop();		
	void Pause();	
	void Continue();
	
	void CreateTaskProcesser(TaskProcesser* newTaskProcesser = NULL);
	Socket* GetListenerContext();
	Server* GetServer();

	TaskProcesser* GetMainTaskProcesser()
	{
		return taskProcesser;
	}

	void SetUseSingleSendDataTaskProcesser(bool isUse)
	{
		useSingleSendDataTaskProcesser = isUse;
	}
	
	void SetListenerContext(Socket* listenerCtx);

	void SetUseCoroutine(bool isUse)
	{
		taskProcesser->SetUseCoroutine(isUse);
	}

	LPVOID GetMainFiberHandle()
	{
		return taskProcesser->GetMainFiberHandle();
	}

	int PostCreateListenerTask();	
	int PostStartInitIocpAcceptTask();
	int PostAcceptedClientTask(Packet* packet);
	int PostSingleIocpAcceptTask(Packet* packet);
	int PostConnectServerTask(ServerInfo* serverInfo, int delay = 0);
	int PostConnectedServerTask(Packet* packet);
	int PostRecvedTask(Packet* packet);

	int PostSendTask(Packet* packet, int delay = 0)
	{
		if(useSingleSendDataTaskProcesser)
			return PostSendDataTask(SendTask, packet, delay);
		return PostTask(SendTask, packet, delay);
	}


	//投递已发送数据任务
	int PostSendedTask(Packet* packet)
	{
		if (useSingleSendDataTaskProcesser)
			return PostSendDataTask(SendedTask, packet);
		return PostTask(SendedTask, packet);
	}

	int PostHeartBeatCheckTask();
	int PostErrorTask(Packet* packet, int delay = 0);
	int PostSocketErrorTask(Socket* socketCtx, int delay = 0);


	int  PostTask(TaskCallBack processDataCallBack, void* taskData, int delay = 0)
	{
		return _PostTaskData(processDataCallBack, NULL, taskData, delay);
	}

	int  PostTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return _PostTaskData(processDataCallBack, releaseDataCallBack, taskData, delay);
	}

	int  PostSendDataTask(TaskCallBack processDataCallBack, void* taskData, int delay = 0)
	{
		return _PostSendTaskData(processDataCallBack, NULL, taskData, delay);
	}

	int  PostSendDataTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return _PostSendTaskData(processDataCallBack, releaseDataCallBack, taskData, delay);
	}

	Socket* GetSocket(socket_id_t socketID)
	{
		auto it = socketMap->find(socketID);
		if (it != socketMap->end()) {
			return it->second;
		}
		return nullptr;
	}

	SocketHashMap* GetSocketMap()
	{
		return socketMap;
	}

	int CheckingPacketVaild(Packet* packet);

protected:

	bool CreateListener();
	bool ConnectServer(ServerInfo* remoteServerInfo);
	void HeartBeatCheck();

	//移除socket
	void RemoveSocketMap();
	void RemoveSocket(socket_id_t socketID);
	void DirectRemoveSocket(Socket *socketCtx);
	
	Socket* GetReusedSocket();
	void RemoveReusedSocketList();

	int SocketError(socket_id_t socketID);
	int SocketError(Socket* socket);
	void* CreateData(socket_id_t socketID);

	void ReleaseData(void* data)
	{
		FREE(data);
	}

	int _PostTaskData(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return taskProcesser->PostTask(processDataCallBack, releaseDataCallBack, taskData, TMSG_DATA, delay);
	}

	int _PostSendTaskData(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return sendTaskProcesser->PostTask(processDataCallBack, releaseDataCallBack, taskData, TMSG_DATA, delay);
	}


	static void* HeartBeatCheckTask(void* data);
	static void* ErrorTask(void* data);
	static void* SocketErrorTask(void* data);

	static void* CreateListenerTask(void* data);
	static void* StartInitIocpAcceptTask(void* data);
	static void* AcceptedClientTask(void* data);
	static void* SingleIocpAcceptTask(void* data);
	static void* RecvedTask(void* data);
	static void* SendTask(void* data);
	static void* SendedTask(void* data);
	static void* ConnectServerTask(void* data);
	static void* ConnectedServerTask(void* data);

	static void* PasueSendTask(void* data);

	static void* ProcessSendedMsgTask(void* data);

private:

	SocketHashMap*                       socketMap;
	TaskProcesser*                       taskProcesser;
	TaskProcesser*                       sendTaskProcesser;                   
	ServerTaskMgr*                       serverTaskMgr;
	SocketCtxList*                       reusedSocketList;           // 可重用的socketCtx列表 

	CmiWaitSem                           socketErrorWaitSem;
	bool                                 useSingleSendDataTaskProcesser;
	bool                                 useSendedEvent;

	socket_id_t                          curtCheckClientSocketKey;
	int                                  perCheckClientCount;


	friend class DePacketor;
	friend class Socket;
	friend class ServerTaskMgr;

};