#pragma once

#include"core/iocp/include/ServerBaseTypes.h"
#include"utils/include/TaskProcesser.h"
#include"core/iocp/include/ServerTask.h"


class ServerTaskMgr : public CmiNewMemory
{
public:
	ServerTaskMgr(Server* _serverCtx);
	~ServerTaskMgr();

	ServerTask* GetServerTask(int serverTaskIdx)
	{
		if (serverTaskIdx > serverTaskCount || serverTaskIdx < 0)
			return nullptr;
		return serverTaskList[serverTaskIdx];
	}

	

	int GetServerTaskCount()
	{
		return serverTaskCount;
	}

	void CreateServerTaskProcess(int serverTaskIdx, TaskProcesser* taskProcesser);
	int Start();
	void Stop();
	int StartListener();
	int ConnectServer(ServerInfo* serverInfo, int delay = 0);

	void SetUseSingleSendTaskProcesser(bool isUse);
	
	int PostSingleIocpAccpetTask(Packet* packet);


	int PostServerMessage(ServerMessage serverMsg, void* data)
	{
		ServerMsgTaskData* stateData = (ServerMsgTaskData*)MALLOC(sizeof(ServerMsgTaskData));
		stateData->msg = serverMsg;
		stateData->dataPtr = data;
		stateData->serverTaskMgr = this;
		return PostTask(StateProcessTask, stateData);
	}

	int  PostTask(TaskCallBack processDataCallBack, void* taskData, int delay = 0)
	{
		return _PostTaskData(processDataCallBack, NULL, taskData, delay);
	}

	int  PostTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return _PostTaskData(processDataCallBack, releaseDataCallBack, taskData, delay);
	}

	int  PostTaskToServerTaskLine(TaskCallBack processDataCallBack, void* taskData, int delay)
	{
		return serverTaskList[AssignServerTaskContextIdx()]->PostTask(processDataCallBack, taskData, delay);
	}

	int  PostTaskToServerTaskLine(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return serverTaskList[AssignServerTaskContextIdx()]->PostTask(processDataCallBack, releaseDataCallBack, taskData, delay);
	}
	
private:
	int AssignServerTaskContextIdx();
	int MessageProcess(ServerMessage msg, void* data);

	int _PostTaskData(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, int delay = 0)
	{
		return taskProcesser->PostTask(processDataCallBack, releaseDataCallBack, taskData, TMSG_DATA, delay);
	}

	static void* StateProcessTask(void* data);

private:

	Server* server;
	int serverTaskCount;
	ServerTaskState* serverTaskState;
	ServerTask** serverTaskList;
	TaskProcesser* taskProcesser;


	friend class ServerTask;
};
