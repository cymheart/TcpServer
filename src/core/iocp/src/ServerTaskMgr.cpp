#include"core/iocp/include/ServerTaskMgr.h"
#include"core/iocp/include/ServerTask.h"
#include"core/iocp/include/Server.h"
#include"utils/include/ByteStream.h"


ServerTaskMgr::ServerTaskMgr(Server* _serverCtx)
{
	server = _serverCtx;
	serverTaskCount = server->serverTaskCount;

	serverTaskList = (ServerTask**)MALLOC(sizeof(ServerTask*) * serverTaskCount);
	serverTaskState = (ServerTaskState*)MALLOC(sizeof(ServerTaskState) * serverTaskCount);  

	for (int i = 0; i < serverTaskCount; i++)
	{
		serverTaskList[i] = new ServerTask(this);
	}

	//
	taskProcesser = new CommonTaskProcesser();
}

ServerTaskMgr::~ServerTaskMgr()
{
	Stop();

	RELEASE_REF(taskProcesser);

	for (int i = 0; i < serverTaskCount; i++)
		RELEASE(serverTaskList[i]);

	FREE(serverTaskList);
	FREE(serverTaskState);
}


void ServerTaskMgr::CreateServerTaskProcess(int serverTaskIdx, TaskProcesser* taskProcesser)
{
	if (serverTaskIdx == -1)
	{
		for (int i = 0; i < serverTaskCount; i++)
			serverTaskList[i]->CreateTaskProcesser(taskProcesser);
		return;
	}

	serverTaskList[serverTaskIdx]->CreateTaskProcesser(taskProcesser);
}

void ServerTaskMgr::SetUseSingleSendTaskProcesser(bool isUse)
{
	for (int i = 0; i < serverTaskCount; i++)
		serverTaskList[i]->SetUseSingleSendDataTaskProcesser(isUse);
}

int ServerTaskMgr::Start()
{
	if (taskProcesser == nullptr)
		return -1;
	
	taskProcesser->Start();

	for (int i = 0; i < serverTaskCount; i++)
	{
		serverTaskList[i]->Start();
	}

	return 0;
}

void ServerTaskMgr::Stop()
{
	for (int i = 0; i < serverTaskCount; i++)
		serverTaskList[i]->Stop();

	taskProcesser->Stop();
}


int ServerTaskMgr::StartListener()
{	
	return serverTaskList[0]->PostCreateListenerTask();
}

int ServerTaskMgr::ConnectServer(ServerInfo* serverInfo, int delay)
{
	ByteStream sendStream;
	sendStream.SetWriteHostToNet(false);

	ServerInfo* cServerInfo = new ServerInfo();
	cServerInfo->Copy(*serverInfo);

	sendStream.Write((void*)cServerInfo);
	sendStream.Write(delay);
	void* data = sendStream.TakeBuf();
	return PostServerMessage(SMSG_REQUEST_CONNECT_SERVER, data);
}

int ServerTaskMgr::PostSingleIocpAccpetTask(Packet* packet)
{
	return serverTaskList[0]->PostSingleIocpAcceptTask(packet);
}

int ServerTaskMgr::AssignServerTaskContextIdx()
{
	int minClientSizeIdx = 0;

	for (int i = 1; i < serverTaskCount; i++)
	{
		if (serverTaskState[i].clientSize <
			serverTaskState[minClientSizeIdx].clientSize)
		{
			minClientSizeIdx = i;
		}
	}

	return minClientSizeIdx;
}


void* ServerTaskMgr::StateProcessTask(void* data)
{
	ServerMsgTaskData* msgData = (ServerMsgTaskData*)data;
	msgData->serverTaskMgr->MessageProcess(msgData->msg,  msgData->dataPtr);
	FREE(msgData);
	return 0;
}

int ServerTaskMgr::MessageProcess(ServerMessage msg, void* data)
{
	int idx;

	switch (msg)
	{
	case SMSG_LISTENER_CREATE_FINISH:
		serverTaskList[0]->PostStartInitIocpAcceptTask();

		if (server->isCheckHeartBeat)
		{
			for (int i = 0; i < serverTaskCount; i++)	
				serverTaskList[i]->PostHeartBeatCheckTask();
		}	
		break;
		
	case SMSG_ACCEPT_CLIENT:
		idx = AssignServerTaskContextIdx();

		serverTaskList[idx]->PostAcceptedClientTask((Packet*)data);
		break;

	case SMSG_REQUEST_CONNECT_SERVER:
	{
		ServerInfo* serverInfo;
		int delay = 0;

		ByteStream recvStream((uint8_t*)data);
		recvStream.SetReadNetToHost(false);
		recvStream.Read((void**)&serverInfo);
		recvStream.Read(&delay);

		idx = AssignServerTaskContextIdx();
		serverTaskList[idx]->PostConnectServerTask(serverInfo, delay);

		RELEASE(serverInfo);
		FREE(data);
	}	
	break;

	default:
		break;
	}

	return 0;
}