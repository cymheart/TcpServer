#include"core/iocp/include/ServerTask.h"
#include"core/iocp/include/ServerInfo.h"
#include<stdlib.h>
#include"core/iocp/include/Server.h"
#include"core/iocp/include/ServerTaskMgr.h"
#include"utils/include/ByteStream.h"


ServerTask::ServerTask(ServerTaskMgr* serverTaskMgr)
	: taskProcesser(NULL)
	, serverTaskMgr(serverTaskMgr)
	, sendTaskProcesser(NULL)
	, useSingleSendDataTaskProcesser(true)
	, curtCheckClientSocketKey(0)
	, perCheckClientCount(10000)
	, useSendedEvent(false)
{
	socketMap = new SocketHashMap(1000000);
	reusedSocketList = new SocketCtxList;

	CreateTaskProcesser();	
}


ServerTask::~ServerTask(void)
{
	Stop();

	RELEASE_REF(taskProcesser);
	RELEASE(sendTaskProcesser);
	RELEASE(reusedSocketList);
	RELEASE(socketMap);
}

void ServerTask::CreateTaskProcesser(TaskProcesser* newTaskProcesser)
{
	Stop();

	if (useSingleSendDataTaskProcesser)
		sendTaskProcesser = new CommonTaskProcesser;

	Server* server = GetServer();
	if (newTaskProcesser == nullptr || server->useDefTaskProcesser)
	{
		taskProcesser = new CommonTaskProcesser;
	}
	else
	{
		RELEASE_REF(taskProcesser);
		taskProcesser = newTaskProcesser;
		taskProcesser->addRef();
	}
}

void ServerTask::Start()
{
	if (taskProcesser)	
		taskProcesser->Start();

	if (sendTaskProcesser)
		sendTaskProcesser->Start();
}

void ServerTask::Stop()
{
	if (taskProcesser)	
		taskProcesser->Stop();

	if (sendTaskProcesser)
		sendTaskProcesser->Stop();

	RemoveSocketMap();
	RemoveReusedSocketList();
}

void ServerTask::Pause()
{
	if (taskProcesser)	
		taskProcesser->Pause();

	if (sendTaskProcesser)
		sendTaskProcesser->Pause();
}

void ServerTask::Continue()
{
	if (taskProcesser)	
		taskProcesser->Continue();

	if (sendTaskProcesser)
		sendTaskProcesser->Continue();
}

Socket* ServerTask::GetListenerContext()
{
   return serverTaskMgr->server->listenCtx;
}

Server* ServerTask::GetServer()
{
	return serverTaskMgr->server;
}


void ServerTask::SetListenerContext(Socket* listenerCtx)
{
	serverTaskMgr->server->listenCtx = listenerCtx;
}



void ServerTask::RemoveSocketMap()
{
	Socket* socketCtx;
	SocketHashMap::iterator it;

	if (socketMap->size() == 0)
		return;

	for (it = socketMap->begin(); it != socketMap->end();)
	{
		socketCtx = it->second;
		RELEASE(socketCtx);
		socketMap->erase(it++);
	}
}


//投递生成监听器任务
int ServerTask::PostCreateListenerTask()
{
	return PostTask(CreateListenerTask,this);
}

void* ServerTask::CreateListenerTask(void* data)
{
	ServerTask* serverTask = (ServerTask*)data;
	serverTask->CreateListener();
	return 0;
}

// 生成监听器
bool ServerTask::CreateListener()
{
	bool ret;
	bool reused = false;
	Server* server = GetServer();
	Socket* listenCtx = NULL;

	// 生成用于监听的Socket的信息
	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	if (server->isReusedSocket)
	{
		listenCtx = GetReusedSocket();
		reused = true;
	}

	if (listenCtx == 0)
	{
		SOCKET sock = TcpWSASocket();
		listenCtx = new Socket(this);
		listenCtx->sock = sock;
		reused = false;
	}

	listenCtx->SetSocketType(LISTENER_SOCKET);
	
	if (INVALID_SOCKET == listenCtx->sock)
	{		
		listenCtx->SetSocketState(INIT_FAILD);
		SocketError(listenCtx);
		//LOG4CPLUS_ERROR(log.GetInst(), "初始化Socket失败，错误代码:"<<_GetLastError());
		return false;
	}

	SetListenerContext(listenCtx);
	(*socketMap)[listenCtx->GetID()] = listenCtx;
//	printf("listenctx=%u, listen_sock = %u\n", listenCtx, listenCtx->sock);

	if (!reused)
	{
		// 将Listen Socket绑定至iocp或epoll中
		ret = server->AssociateSocketContext(listenCtx);

		if (false == ret)
		{
			//LOG4CPLUS_ERROR(log.GetInst(), "绑定iocp失败！错误代码:" <<_GetLastError());
			listenCtx->SetSocketState(ASSOCIATE_FAILD);
			SocketError(listenCtx);
			return false;
		}
	}

	// 生成本机绑定地址信息
	SOCKADDR_IN localAddress;

	if (false == CreateAddressInfoEx(server->localIP, server->localListenPort, &localAddress))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "无效的本机监听地址["<<localIP.c_str()<<":"<<localListenPort<<"]");
		return false;
	}
	else {
		//LOG4CPLUS_ERROR(log.GetInst(), "本机监听地址["<<localIP.c_str()<<":"<<localListenPort<<"]");
	}

	// 绑定地址和端口
	if (SOCKET_ERROR == ::bind(listenCtx->sock, (struct sockaddr *) &localAddress, sizeof(localAddress)))
	{
		if (WSAGetLastError() == WSAEADDRINUSE)
		{
			printf("端口被使用!(%d)\n", server->localListenPort);
			listenCtx->SetSocketState(PORT_BEUSED);
			SocketError(listenCtx);
		}
		else
		{
			listenCtx->SetSocketState(BIND_FAILD);
			SocketError(listenCtx);
		}
		return false;
	}

	// 开始进行监听
	if (SOCKET_ERROR == listen(listenCtx->sock, SOMAXCONN))
	{
		listenCtx->SetSocketState(LISTEN_FAILD);
		SocketError(listenCtx);
		//LOG4CPLUS_ERROR(log.GetInst(), "Listen()函数执行出现错误.");
		return false;
	}

	listenCtx->SetSocketState(LISTENING);
	serverTaskMgr->PostServerMessage(SMSG_LISTENER_CREATE_FINISH, NULL);

	return true;
}


//投递请求一定数量Accept任务
int ServerTask::PostStartInitIocpAcceptTask()
{
	int ret = PostTask(StartInitIocpAcceptTask, this);
	return ret;
}

void* ServerTask::StartInitIocpAcceptTask(void* data)
{
	ServerTask* serverTask = (ServerTask*)data;
	Server* server = serverTask->GetServer();
	Packet* packet;
	Socket* newSocketCtx;

	// 为AcceptEx 准备参数，然后投递AcceptEx I/O请求
	for (int i = 0; i < MAX_POST_ACCEPT; i++)
	{
		newSocketCtx = new Socket(serverTask);
		newSocketCtx->SetSocketType(LISTEN_CLIENT_SOCKET);
		newSocketCtx->sock = TcpWSASocket(); // 为以后新连入的客户端先准备好Socket

		packet = newSocketCtx->CreatePacket((sizeof(SOCKADDR_IN) + 16) * 2);  // 新建一个Accept常用IO_CONTEXT
		if (false == server->IocpPostAccept(packet))
		{
			Packet::ReleasePacket(packet);
			RELEASE(newSocketCtx);
		}
	}

	return 0;
}


//投递接收到客户端任务
int ServerTask::PostAcceptedClientTask(Packet* packet)
{
	packet->serverTask = this;
	packet->socketCtx->serverTask = this;
	return PostTask(AcceptedClientTask, packet);
}

void* ServerTask::AcceptedClientTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	// 把这个有效的客户端信息，加入到socketMap中去
	(*(serverTask->socketMap))[socketCtx->GetID()] = socketCtx;

	socketCtx->SetPack((uint8_t*)packet->buf, packet->transferedBytes);
	socketCtx->dePacketor->UnPack(EV_SOCKET_ACCEPTED, *socketCtx);
	socketCtx->RemovePack();

	size_t bufSize = socketCtx->dePacketor->GetMaxBufferSize();

	if (packet->packBuf.len != bufSize)
	{
		Packet::ReleasePacket(packet);
		packet = socketCtx->CreatePacket((int)bufSize);
	}

	if (false == server->IocpPostRecv(packet))
	{
		Packet::ReleasePacket(packet);
	}

	return 0;
}



//
int ServerTask::PostSingleIocpAcceptTask(Packet* packet)
{
	packet->serverTask = this;
	return PostTask(SingleIocpAcceptTask, packet);
}

void* ServerTask::SingleIocpAcceptTask(void* data)
{
	Packet* packet = (Packet*)data;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();
	Socket* socketCtx = 0;

	// 投递新的AcceptEx
	if (server->isReusedSocket)
	{
		socketCtx = serverTask->GetReusedSocket();
		socketCtx->SetSocketType(LISTEN_CLIENT_SOCKET);
	}
	else
	{
		socketCtx = new Socket(serverTask);
		socketCtx->SetSocketType(LISTEN_CLIENT_SOCKET);
		socketCtx->sock = TcpWSASocket();
	}

	packet->socketCtx = socketCtx;

	if (false == server->IocpPostAccept(packet))
	{
		Packet::ReleasePacket(packet);
	}
	return 0;
}


//投递连接服务器任务
int ServerTask::PostConnectServerTask(ServerInfo* serverInfo, int delay)
{
	ServerInfo* cServerInfo = new ServerInfo();
	cServerInfo->Copy(*serverInfo);
	cServerInfo->serverTask = this;
	return PostTask(ConnectServerTask, cServerInfo, delay);
}

void* ServerTask::ConnectServerTask(void* data)
{
	ServerInfo* serverInfo = (ServerInfo*)data;
	serverInfo->serverTask->ConnectServer(serverInfo);
	return 0;
}

bool ServerTask::ConnectServer(ServerInfo* serverInfo)
{
	// 生成用于连接服务器的Socket的信息
	Socket* connectSocketCtx = 0;
	Server* server = GetServer();
	uint64_t timeStamp = CmiGetTickCount64();

	// 生成用于监听的Socket的信息
	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	if (server->isReusedSocket)
	{
		connectSocketCtx = GetReusedSocket();
	}

	if (connectSocketCtx == 0)
	{
		SOCKET sock = TcpWSASocket();
		connectSocketCtx = new Socket(this);
		connectSocketCtx->sock = sock;
	}

	connectSocketCtx->SetSocketType(CONNECT_SERVER_SOCKET);
	serverInfo->socketCtx = connectSocketCtx;
	connectSocketCtx->SetRemoteServerInfo(serverInfo);
	connectSocketCtx->dataTransMode = serverInfo->dataTransMode;
	(*socketMap)[connectSocketCtx->GetID()] = connectSocketCtx;

	if (INVALID_SOCKET == connectSocketCtx->sock)
	{
		connectSocketCtx->SetSocketState(INIT_FAILD);
		SocketError(connectSocketCtx);
		//LOG4CPLUS_ERROR(log.GetInst(), "初始化Socket失败，错误代码:" << _GetLastError());
		return false;
	}

	if (connectSocketCtx->GetSocketState() != WAIT_REUSED)
	{
		// 生成本机绑定地址信息
		SOCKADDR_IN localAddress;

		if (false == CreateAddressInfoEx(server->localIP, serverInfo->localConnectPort, &localAddress))
		{
			//LOG4CPLUS_ERROR(log.GetInst(), "无效的本机地址["<<localIP.c_str()<<":"<<remoteServerInfo.localConnectPort<<"]");
			printf("端口地址有问题!(%d)\n", serverInfo->localConnectPort);
			connectSocketCtx->SetSocketState(LOCALIP_INVALID);
			SocketError(connectSocketCtx);
			return false;
		}

		// 绑定地址和端口
		if (SOCKET_ERROR == ::bind(connectSocketCtx->sock, (SOCKADDR *)&localAddress, sizeof(localAddress)))
		{
			if (WSAGetLastError() == WSAEADDRINUSE)
			{
				printf("端口被使用!(%d)\n", serverInfo->localConnectPort);
				connectSocketCtx->SetSocketState(PORT_BEUSED);
				SocketError(connectSocketCtx);
			}
			else
			{
				connectSocketCtx->SetSocketState(BIND_FAILD);
				SocketError(connectSocketCtx);
			}
			
			return false;
		}

		if (false == server->AssociateSocketContext(connectSocketCtx))
		{
			//LOG4CPLUS_ERROR(log.GetInst(), "完成端口绑定 connect Socket失败！错误代码:" <<_GetLastError());
			printf("完成端口绑定有问题!(%d)\n", serverInfo->localConnectPort);
			connectSocketCtx->SetSocketState(ASSOCIATE_FAILD);
			SocketError(connectSocketCtx);
			return false;
		}
	}

	connectSocketCtx->UpdataTimeStamp(timeStamp);
	Packet* newIoCtx = connectSocketCtx->CreatePacket(0);

	// 开始连接服务器
	if (false == server->IocpPostConnect(newIoCtx))
	{
		Packet::ReleasePacket(newIoCtx);
		return false;
	}

	connectSocketCtx->SetSocketState(CONNECTTING_SERVER);

	return true;
}


// 与远程服务器连接上后，进行处理
int ServerTask::PostConnectedServerTask(Packet* packet)
{
	return PostTask(ConnectedServerTask, packet);
}

void* ServerTask::ConnectedServerTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	int optval = 1;
	setsockopt(
		packet->socketCtx->sock,
		SOL_SOCKET,
		SO_UPDATE_CONNECT_CONTEXT,
		(char *)&optval,
		sizeof(optval)
		);

	socketCtx->SetSocketState(CONNECTED_SERVER);
	socketCtx->UpdataTimeStamp();
	
	size_t bufSize = socketCtx->dePacketor->GetMaxBufferSize();

	if (packet->packBuf.len != bufSize) 
	{
		Packet::ReleasePacket(packet);
		packet = socketCtx->CreatePacket((int)bufSize);
	}

	socketCtx->dePacketor->UnPack(EV_SOCKET_CONNECTED, *socketCtx);

	if (false == server->IocpPostRecv(packet))
	{
		Packet::ReleasePacket(packet);
	}

	return 0;
}


//投递接收到数据任务
int ServerTask::PostRecvedTask(Packet* packet)
{
	return PostTask(RecvedTask, packet);
}

void* ServerTask::RecvedTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	if (serverTask->CheckingPacketVaild(packet) == 0)
		return 0;

	socketCtx->UpdataTimeStamp();

	if (socketCtx->dataTransMode == MODE_PACK)
	{
		socketCtx->dePacketor->SetCurtPack(socketCtx, (uint8_t*)packet->buf, packet->transferedBytes);
		int ret = socketCtx->dePacketor->Extract();

		if (ret == 2){
			socketCtx->SetSocketState(RECV_DATA_TOO_BIG);
			serverTask->SocketError(socketCtx->GetID());
		}
	}
	else
	{
		socketCtx->SetPack((uint8_t*)packet->buf, packet->transferedBytes);
		socketCtx->dePacketor->UnPack(EV_SOCKET_RECV, *socketCtx);
		socketCtx->RemovePack();
	}

	// 然后开始投递下一个WSARecv请求
	if (false == server->IocpPostRecv(packet))
	{
		Packet::ReleasePacket(packet);
	}

	return 0;
}

//此任务根据设置可能在其它任务处理线上执行
void* ServerTask::SendTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	if (serverTask->CheckingPacketVaild(packet) == 0)
		return 0;

	PacketList* sendList = socketCtx->sendList;

	if (sendList->empty())
	{
		if (false == server->IocpPostSend(packet))
			Packet::ReleasePacket(packet);
	}
	else
	{
		sendList->push_back(packet);
	}
	

	return 0;
}

//此任务根据设置可能在其它任务处理线上执行
void* ServerTask::SendedTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	if (serverTask->CheckingPacketVaild(packet) == 0)
		return 0;

	if (serverTask->useSendedEvent) {
		if (serverTask->useSingleSendDataTaskProcesser)
			serverTask->PostTask(ProcessSendedMsgTask, serverTask->CreateData(packet->socketID));
		else
			socketCtx->dePacketor->UnPack(EV_SOCKET_SEND, *socketCtx);
	}

	Packet::ReleasePacket(packet);

	PacketList* sendList = socketCtx->sendList;	
	if (sendList->size() == 0)
		return 0;

	PacketList::iterator iter = sendList->begin();
	packet = *iter;
	if (packet){
		sendList->erase(iter);
		if (false == server->IocpPostSend(packet))
			Packet::ReleasePacket(packet);
	}

	return 0;
}


void* ServerTask::ProcessSendedMsgTask(void* data)
{
	ServerTask* serverTask;
	Socket* socket;
	socket_id_t socketID;
	ByteStream dataStream((uint8_t*)data);
	dataStream.SetReadNetToHost(false);
	dataStream.Read((void**)&serverTask);
	dataStream.Read(socketID);

	if (serverTask->GetSocket(socketID) == nullptr)
		return 0;
	
	socket = (*serverTask->socketMap)[socketID];
	socket->dePacketor->UnPack(EV_SOCKET_SEND, *socket);
	return 0;
}


//投递心跳检测任务
int ServerTask::PostHeartBeatCheckTask()
{
	Server* server = GetServer();
	return PostTask(HeartBeatCheckTask, this, server->childLifeDelayTime - 1000);
}

void* ServerTask::HeartBeatCheckTask(void* data)
{
	ServerTask* serverTask = (ServerTask*)data;
	Server* server = serverTask->GetServer();

	if (server->isCheckHeartBeat)
	{
		serverTask->HeartBeatCheck();
		serverTask->PostHeartBeatCheckTask();
	}

	return 0;
}

void ServerTask::HeartBeatCheck()
{
	Socket* socketCtx;
	uint64_t time = CmiGetTickCount64();
	uint64_t tm;
	Server* server = GetServer();

	SocketHashMap::iterator it, next;
	int count = 0;

	if (curtCheckClientSocketKey != 0)
		it = socketMap->find(curtCheckClientSocketKey);
	else
		it = socketMap->begin();

	for (; it != socketMap->end(); count++)
	{
		if (count > perCheckClientCount)
			break;

		socketCtx = it->second;
		tm = time - socketCtx->timeStamp;

		if (tm > server->childLifeDelayTime)
		{
			next = ++it;
			socketCtx->SetSocketState(RESPONSE_TIMEOUT);
			SocketError(socketCtx->GetID());

			it = next;
			continue;
		}
		
		it++;
	}

	if (it == socketMap->end()) {
		curtCheckClientSocketKey = 0;
	}
	else
	{
		curtCheckClientSocketKey = it->first;
	}
}


//投递错误消息任务
int ServerTask::PostErrorTask(Packet* packet, int delay)
{
	return PostTask(ErrorTask, packet, delay);
}

void* ServerTask::ErrorTask(void* data)
{
	Packet* packet = (Packet*)data;
	Socket* socketCtx = packet->socketCtx;
	ServerTask* serverTask = packet->serverTask;
	Server* server = serverTask->GetServer();

	if (serverTask->CheckingPacketVaild(packet) == 0)
		return 0;

	serverTask->SocketError(socketCtx);
	return 0;
}


int ServerTask::PostSocketErrorTask(Socket* socketCtx, int delay)
{
	void* data = CreateData(socketCtx->GetID());
	return PostTask(SocketErrorTask, data, delay);
}

void* ServerTask::SocketErrorTask(void* data)
{
	ByteStream dataStream((uint8_t*)data);
	dataStream.SetReadNetToHost(false);
	ServerTask* serverTask;
	socket_id_t socketID;
	dataStream.Read((void**)&serverTask);
	dataStream.Read(socketID);

	serverTask->SocketError(socketID);
	serverTask->ReleaseData(data);
	return 0;
}


int ServerTask::SocketError(socket_id_t socketID)
{
	Socket* socket = GetSocket(socketID);
	if (socket == nullptr)
		return 0;

	SocketError(socket);
	return 0;
}

int ServerTask::SocketError(Socket* socket)
{
	if (useSingleSendDataTaskProcesser)
	{
		sendTaskProcesser->PostTask(PasueSendTask, NULL, this, TMSG_PAUSE);
		socket->dePacketor->UnPack(EV_SOCKET_OFFLINE, *socket);
		socketErrorWaitSem.WaitSem(-1);
		RemoveSocket(socket->GetID());
		sendTaskProcesser->Continue();
	}
	else
	{
		socket->dePacketor->UnPack(EV_SOCKET_OFFLINE, *socket);
		RemoveSocket(socket->GetID());
	}
	return 0;
}

void* ServerTask::PasueSendTask(void* data)
{
	ServerTask* serverTask = (ServerTask*)data;
	serverTask->socketErrorWaitSem.SetSem();
	return 0;
}

void ServerTask::RemoveSocket(socket_id_t socketID)
{
	SocketHashMap::iterator it, next;
	it = socketMap->find(socketID);
	if (it != socketMap->end())
	{
		if (it->first == curtCheckClientSocketKey)
		{
			next = it;
			next++;

			if (next != socketMap->end())
				curtCheckClientSocketKey = next->first;
			else
				curtCheckClientSocketKey = 0;
		}

		DirectRemoveSocket(it->second);
		socketMap->erase(it);
	}
}

void ServerTask::DirectRemoveSocket(Socket *socketCtx)
{
	Server* server = GetServer();

	socketCtx->Release();
	if (!server->isReusedSocket)	
		RELEASE(socketCtx);
}

Socket* ServerTask::GetReusedSocket()
{
	Server* server = GetServer();
	if (!server->isReusedSocket || reusedSocketList->size() == 0)
		return 0;

	SocketCtxList::iterator iter = reusedSocketList->begin();
	Socket* socketCtx = *iter;
	socketCtx->ResetGenID();
	reusedSocketList->erase(iter);
	return socketCtx;
}

void ServerTask::RemoveReusedSocketList()
{
	SocketCtxList::iterator iter = reusedSocketList->begin();
	Socket* cur;

	if (reusedSocketList->size() != 0)
	{
		for (iter = reusedSocketList->begin(); iter != reusedSocketList->end();)
		{
			cur = (*iter);
			RELEASE(cur);
			iter = reusedSocketList->erase(iter);
		}
	}
}


int ServerTask::CheckingPacketVaild(Packet* packet)
{
	if (GetSocket(packet->socketID) == nullptr)
	{
		Packet::ReleasePacket(packet);
		return 0;
	}
	return 1;
}


void* ServerTask::CreateData(socket_id_t socketID)
{	
	ByteStream byteStream;
	byteStream.SetWriteHostToNet(false);
	ServerTask* serverTask = this;
	byteStream.Write((void*)serverTask);
	byteStream.Write(socketID);
	return byteStream.TakeBuf();
}
