#include"core/iocp/include/Server.h"
#include"core/iocp/include/ServerInfo.h"
#include"core/iocp/include/ServerTaskMgr.h"
#include"core/iocp/include/ServerTask.h"


extern LPFN_CONNECTEX               lpfnConnectEx;
extern LPFN_DISCONNECTEX            lpfnDisConnectEx;
extern LPFN_ACCEPTEX                lpfnAcceptEx;               // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
extern LPFN_GETACCEPTEXSOCKADDRS    lpfnGetAcceptExSockAddrs;

Server::Server()
	: nThreads(0)
	, listenCtx(NULL)
	, ioThreads(NULL)
	, useDefTaskProcesser(false)
	, isCheckHeartBeat(false)
	, isStop(true)
	, isReusedSocket(false)
	, childLifeDelayTime(11000)             //10s
	, iocp(NULL)                                
	, localListenPort(DEFAULT_PORT)
	, exitIoThreads(0)
	, serverTaskCount(1)
	, useSingleSendTaskProcesser(true)
	, dePacketor(nullptr)
{
	LoadSocketLib();
	GetLocalIPEx(localIP); 
	serverTaskMgr = new ServerTaskMgr(this);

	SetUseSingleSendTaskProcesser(useSingleSendTaskProcesser);
	UniqueID::GetInstance();
}

Server::~Server(void)
{
	Stop();

	RELEASE(serverTaskMgr);
	RELEASE_REF(dePacketor);

	UnloadSocketLib();
}

//	启动服务器
bool Server::Start()
{
	if (!isStop)
		return true;

	// 初始化IOCP
	if (false == InitializeIOCP())
		return false;

	serverTaskMgr->Start();

	isStop = false;

	return true;
}


//	开始发送系统退出消息，退出完成端口和线程资源
void Server::Stop()
{
	if (isStop)
		return;

	InterlockedExchange(&exitIoThreads, TRUE);

	for (int i = 0; i < nThreads; i++) {
		// 通知所有的完成端口操作退出
		PostQueuedCompletionStatus(iocp, 0, (DWORD)EXIT_CODE, NULL);
	}

	// 关闭IOCP句柄
	RELEASE_HANDLE(iocp);

	serverTaskMgr->Stop();


	//
	for each(auto item in msgCoroutineMap)
	{
		RELEASE(item.second);
	}

	isStop = true;
}


// 初始化WinSock 2.2
bool Server::LoadSocketLib()
{
	WORD wVersionRequested;
	WSADATA wsaData;    // 这结构是用于接收Windows Socket的结构信息的
	int nResult;

	wVersionRequested = MAKEWORD(2, 2);   // 请求2.2版本的WinSock库
	nResult = WSAStartup(wVersionRequested, &wsaData);

	if (NO_ERROR != nResult)
	{
		return false;          // 返回值为零的时候是表示成功申请WSAStartup
	}

	LoadIocpExFuncs();
	return true;
}


void Server::SetServerTaskProcess(int serverTaskIdx, TaskProcesser* taskProcesser)
{
	if (!isStop)
		return;

	serverTaskMgr->CreateServerTaskProcess(serverTaskIdx, taskProcesser);
}

Timer* Server::CreateTimer(int serverTaskIdx, TimerCallBack timerCB, void* param, int durationMS)
{
	if (isStop)
		return nullptr;

	ServerTask* serverTask = serverTaskMgr->GetServerTask(serverTaskIdx);
	TaskProcesser* taskProcesser = serverTask->GetMainTaskProcesser();
	Timer* timer = new Timer(taskProcesser, durationMS, timerCB, param);
	return timer;
}


void Server::SetUnPackCallBack(UnPackCallBack _unPackCallBack, void* param)
{
	if (isStop && dePacketor != NULL)
		dePacketor->SetUnPackCallBack(_unPackCallBack, param);
}

inline void Server::SetDePacketor(DePacketor* _dePacketor)
{
	if (!isStop || _dePacketor == nullptr)
		return;

	if (_dePacketor == dePacketor)
		return;

	if (dePacketor != nullptr)
		DEBUG("%s:%I64u,减少引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef() - 1);

	RELEASE_REF(dePacketor);
	dePacketor = _dePacketor;
	dePacketor->addRef();

	if (dePacketor != nullptr)
		DEBUG("%s:%I64u,增加引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef());
}

void Server::SetServerMachineID(int machineID)
{
	UniqueID::GetInstance().set_workid(machineID);
}

void Server::SetUseSingleSendTaskProcesser(bool isUse)
{
	if (!isStop)
		return;

	useSingleSendTaskProcesser = isUse;
	
	if (serverTaskMgr != NULL)
		serverTaskMgr->SetUseSingleSendTaskProcesser(isUse);
}

Socket* Server::GetSocket(uint64_t socketID, int searchInServerTaskIdx)
{
	if (searchInServerTaskIdx != -1)
	{
		ServerTask* serverTask = serverTaskMgr->GetServerTask(searchInServerTaskIdx);
		return serverTask->GetSocket(socketID);
	}
	
	ServerTask* serverTask;
	Socket* socket;
	for (int i = 0; i < serverTaskMgr->GetServerTaskCount(); i++)
	{
		serverTask = serverTaskMgr->GetServerTask(i);
		socket = serverTask->GetSocket(socketID);	
		if (socket != nullptr)
			return socket;
	}

	return nullptr;
}

void Server::SetUseCoroutine(int serverTaskIdx, bool isUse)
{
	if (!isStop)
		return;

	ServerTask* serverTask;
	if (serverTaskIdx != -1)
	{
		serverTask = serverTaskMgr->GetServerTask(serverTaskIdx);
		serverTask->SetUseCoroutine(isUse);
		return;
	}

	for (int i = 0; i < serverTaskMgr->GetServerTaskCount(); i++)
	{
		serverTask = serverTaskMgr->GetServerTask(i);
		serverTask->SetUseCoroutine(isUse);
	}
}

LPVOID Server::GetMainFiberHandle(int serverTaskIdx)
{
	ServerTask* serverTask;
	LPVOID mainFiberHandle;

	if (serverTaskIdx >= 0 && serverTaskIdx < serverTaskMgr->GetServerTaskCount())
	{
		serverTask = serverTaskMgr->GetServerTask(serverTaskIdx);
		mainFiberHandle = serverTask->GetMainFiberHandle();
		return mainFiberHandle;
	}

	return nullptr;
}

void Server::SaveMsgCoroutine(BaseMsgCoroutine* co)
{
	LPVOID coHandle = co->GetHandle();
	uint64_t key = (uint64_t)coHandle;
	msgCoroutineMap[key] = co;
}

void Server::DelMsgCoroutine(BaseMsgCoroutine* co)
{
	LPVOID coHandle = co->GetHandle();
	msgCoroutineMap.erase((uint64_t)coHandle);
	RELEASE(co);
}


BaseMsgCoroutine* Server::GetMsgCoroutine(LPVOID coHandle)
{
	auto it = msgCoroutineMap.find((uint64_t)coHandle);
	if (it != msgCoroutineMap.end())
		return it->second;
	return nullptr;
}

void Server::MsgCoroutineYield()
{
	LPVOID mainCoHandle = GetMainFiberHandle();
	BaseMsgCoroutine::CoYield(mainCoHandle);
}

int Server::GetTotalSocketCount()
{
	ServerTask* serverTask;
	SocketHashMap* socketMap;
	int totalCount = 0;
	for (int i = 0; i < serverTaskMgr->GetServerTaskCount(); i++)
	{
		serverTask = serverTaskMgr->GetServerTask(i);
		socketMap = serverTask->GetSocketMap();
		totalCount += socketMap->size();
	}

	return totalCount;
}

int Server::StartListener()
{
	Start();	
	return serverTaskMgr->StartListener();
}

int Server::ConnectServer(ServerInfo* serverInfo, int delay)
{
	Start();
	return serverTaskMgr->ConnectServer(serverInfo, delay);
}


//工作者线程:为IOCP请求服务的工作者线程
//也就是每当完成端口上出现了完成数据包，就将之取出来进行处理的线程
DWORD WINAPI Server::IoThread(LPVOID pParam)
{
	THREADPARAMS_WORKER* param = (THREADPARAMS_WORKER*)pParam;
	Server* server = (Server*)param->server;
	int nThreadNo = (int)param->nThreadNo;

	////LOG4CPLUS_INFO(log.GetInst(), "工作者线程启动，ID:" << nThreadNo);

	OVERLAPPED      *pOverlapped = NULL;
	Socket   *socketCtx = NULL;
	DWORD            dwBytesTransfered = 0;

	// 循环处理请求，直到接收到Shutdown信息为止
	while (server->exitIoThreads != TRUE)
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			server->iocp,
			&dwBytesTransfered,
			(PULONG_PTR)&socketCtx,
			&pOverlapped,
			INFINITE);

		// 如果收到的是退出标志，则直接退出
		if (EXIT_CODE == socketCtx)
		{
			break;
		}
		// 判断是否出现了错误
		else if (!bReturn)
		{
			// 读取传入的参数
			Packet* packet = CONTAINING_RECORD(pOverlapped, Packet, overlapped);

			//LOG4CPLUS_ERROR(log.GetInst(), "GetQueuedCompletionStatus()非正常返回! 服务器将关闭问题SOCKET:"<<socketCtx);
			//LOG4CPLUS_ERROR(log.GetInst(), "这个可能是由于"<<socketCtx<<"非正常关闭造成的.错误代码:" << WSAGetLastError());

			packet->serverTask->PostErrorTask(packet);
			continue;
		}
		else
		{
			// 读取传入的参数
			Packet* packet = CONTAINING_RECORD(pOverlapped, Packet, overlapped);
			ServerTask* serverTask = packet->serverTask;


			// 判断是否有客户端断开了
			if (dwBytesTransfered == 0 &&
				packet->postIoType != POST_IO_CONNECT &&
				packet->postIoType != POST_IO_ACCEPT)
			{
				serverTask->PostErrorTask(packet);
			}
			else
			{
				packet->transferedBytes = dwBytesTransfered;

				switch (packet->postIoType)
				{
				case POST_IO_CONNECT:
					serverTask->PostConnectedServerTask(packet);
					break;

				case POST_IO_ACCEPT:
					server->DoAccept(packet);
					break;

				case POST_IO_RECV:
					serverTask->PostRecvedTask(packet);
					break;

				case POST_IO_SEND:
					serverTask->PostSendedTask(packet);
					break;
				}
			}
		}

	}

	//LOG4CPLUS_TRACE(log.GetInst(), "工作者线程" <<nThreadNo<< "号退出.");
	// 释放线程参数
	RELEASE(pParam);

	return 0;
}

// 初始化完成端口
bool Server::InitializeIOCP()
{
	// 建立第一个完成端口
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == iocp)
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "建立完成端口失败！错误代码:"<<WSAGetLastError());
		return false;
	}

	// 根据本机中的处理器数量，建立对应的线程数
	nThreads = 5; // IO_THREADS_PER_PROCESSOR * GetNumProcessors();

	// 为工作者线程初始化句柄
	ioThreads = new HANDLE[nThreads];

	// 根据计算出来的数量建立工作者线程
	for (int i = 0; i < nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->server = this;
		pThreadParams->nThreadNo = i + 1;

		ioThreads[i] = CreateThread(NULL, 0, IoThread, (void*)pThreadParams, 0, NULL);
	}

	return true;
}



//投递Connect请求
bool Server::IocpPostConnect(Packet* packet)
{
	// 初始化变量
	DWORD dwBytes = 0;
	OVERLAPPED *p_ol = &packet->overlapped;
	PackBuf *p_wbuf = &packet->packBuf;
	Socket* socket = packet->socketCtx;
	ServerInfo* serverInfo = socket->GetRemoteServerInfo();

	// 生成远程服务器地址信息
	SOCKADDR_IN serverAddress;
	if (false == CreateAddressInfoEx(serverInfo->serverIP, serverInfo->serverPort, &serverAddress))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "无效的远程服务器地址["<<remoteServerInfo.serverIP.c_str()<<":"<<remoteServerInfo.serverPort<<"]");
		return false;
	}

	packet->postIoType = POST_IO_CONNECT;

	int rc = lpfnConnectEx(
		packet->socketCtx->sock,
		(sockaddr*)&(serverAddress),
		sizeof(serverAddress),
		p_wbuf->buf,
		p_wbuf->len,
		&dwBytes,
		p_ol);

	if (rc == FALSE)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			//LOG4CPLUS_ERROR(log.GetInst(), "投递 ConnectEx 请求失败，错误代码:" << WSAGetLastError());
			return false;
		}
	}

	return true;
}

// 往iocp投递Accept请求
bool Server::IocpPostAccept(Packet* packet)
{
	// 准备参数
	DWORD dwBytes = 0;
	packet->postIoType = POST_IO_ACCEPT;
	PackBuf *p_wbuf = &packet->packBuf;
	OVERLAPPED *p_ol = &packet->overlapped;

	if (INVALID_SOCKET == packet->socketCtx->sock)
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "创建用于Accept的Socket失败！错误代码:"<< WSAGetLastError());
		return false;
	}

	// 投递AcceptEx
	if (FALSE == lpfnAcceptEx(
		listenCtx->sock,
		packet->socketCtx->sock,
		p_wbuf->buf,
		0,                              //p_wbuf->len - ((sizeof(SOCKADDR_IN)+16) * 2)
		sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16,
		&dwBytes,
		p_ol))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			//LOG4CPLUS_ERROR(log.GetInst(), "投递 AcceptEx 请求失败，错误代码:" << WSAGetLastError());
			return false;
		}
	}

	return true;
}


// 投递接收数据请求
bool Server::IocpPostRecv(Packet* packet)
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	//PackBuf p_wbuf ;
	PackBuf *p_wbuf = &(packet->packBuf);
	OVERLAPPED *p_ol = &packet->overlapped;

	//Packet::ClearBuffer(packet);
	packet->postIoType = POST_IO_RECV;

	// 初始化完成后，投递WSARecv请求
	int nBytesRecv = WSARecv(packet->socketCtx->sock, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL);

	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "投递WSARecv失败!");
		return false;
	}

	return true;
}


// 投递发送数据请求
bool Server::IocpPostSend(Packet* packet)
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	PackBuf *p_wbuf = &(packet->packBuf);
	OVERLAPPED *p_ol = &packet->overlapped;

	packet->postIoType = POST_IO_SEND;

	// 初始化完成后，投递WSARecv请求
	int nBytesRecv = WSASend(packet->socketCtx->sock, p_wbuf, 1, &dwBytes, dwFlags, p_ol, NULL);

	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "投递WSASend失败!");
		return false;
	}

	return true;
}

// 将句柄(Socket)绑定到完成端口中
bool Server::AssociateSocketContext(Socket *socketCtx)
{
	// 将用于和客户端通信的SOCKET绑定到完成端口中
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)socketCtx->sock, iocp, (ULONG_PTR)socketCtx, 0);

	if (NULL == hTemp)
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "执行CreateIoCompletionPort()出现错误.错误代码："<< GetLastError());
		return false;
	}

	return true;
}

void Server::DoAccept(Packet* packet)
{
	Socket* newSocketCtx = packet->socketCtx;
	int err = 0;

	SOCKADDR_IN* ClientAddr = NULL;
	SOCKADDR_IN* LocalAddr = NULL;
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);
	std::string* value = 0;

	//取得连入客户端的地址信息,通过 lpfnGetAcceptExSockAddrs
	//不但可以取得客户端和本地端的地址信息，还能顺便取出客户端发来的第一组数据
	lpfnGetAcceptExSockAddrs(
		packet->packBuf.buf,
		0,
		sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16,
		(LPSOCKADDR*)&LocalAddr,
		&localLen,
		(LPSOCKADDR*)&ClientAddr,
		&remoteLen);


	err = setsockopt(
		packet->socketCtx->sock,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&(listenCtx->sock),
		sizeof(listenCtx->sock));


	//LOG4CPLUS_INFO(log.GetInst(),"接受来自地址"<<inet_ntoa(ClientAddr->sin_addr)<<"的链接.");
	uint64_t timeStamp = CmiGetTickCount64();
	bool ret = true;

	// 参数设置完毕，将这个Socket和完成端口绑定(这也是一个关键步骤)
	if (newSocketCtx->GetSocketState() != WAIT_REUSED)
		ret = AssociateSocketContext(newSocketCtx);

	newSocketCtx->SetSocketState(CONNECTED_CLIENT);
	newSocketCtx->UpdataTimeStamp(timeStamp);

	// 取得客户端ip和端口号
	inet_ntop(AF_INET, (void *)&(ClientAddr->sin_addr), newSocketCtx->remoteIP, 16);
	newSocketCtx->remotePort = ClientAddr->sin_port;


	if (ret == true)
	{	
		Packet* packet = newSocketCtx->CreatePacket((int)dePacketor->GetMaxBufferSize());
		serverTaskMgr->PostServerMessage(SMSG_ACCEPT_CLIENT, packet);
	}
	else
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "绑定操作任务失败.重新投递Accept");
		newSocketCtx->SetSocketState(ASSOCIATE_FAILD);
		RELEASE(newSocketCtx);
		return;
	}

	serverTaskMgr->PostSingleIocpAccpetTask(packet);
}
