#pragma once

#include"utils/include/IocpExFuncs.h"
#include"utils/include/utils.h"
#include"utils/include/CmiThreadLock.h"
#include"utils/include/CmiWaitSem.h"
#include"utils/include/CmiAlloc.h"
#include"utils/include/CmiNewMemory.h"
#include<string>
#include<map>
#include<set>
#include <iostream>
#include<list>
#include <unordered_map>  
#include"utils/include/Singleton.h"
#include"utils/include/UniqueID.h"
#include"utils/include/CmiLog.h"
using namespace std;

	
// 每一个处理器上产生多少个线程
#define IO_THREADS_PER_PROCESSOR 2
// 同时投递的Accept请求的数量
#define MAX_POST_ACCEPT              20

#define ONE_DAY_MS        86400000

#define MAX_PATH          260
// 缓冲区长度 (1024*4)
#define MAX_BUFFER_LEN        2048
#define MIDDLE_BUFFER_LEN     1024
// 默认端口
#define DEFAULT_PORT          8888
// 默认IP地址
#define DEFAULT_IP            "127.0.0.1"

// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL


// 释放Socket宏
//#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { lpfnDisConnectEx(x,NULL,TF_REUSE_SOCKET,0);x=INVALID_SOCKET;}}
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { shutdown(x,SD_SEND); closesocket(x);x=INVALID_SOCKET;}}

typedef uint64_t socket_id_t;

	typedef struct _Packet Packet;
	class DePacketor;
	class ServerInfo;
	class Server;
	class Socket;
	class ServerTask;
	class ServerTaskMgr;
	class BaseMsgProcesser;
	class BaseMsgCoroutine;

	typedef basic_string<char, char_traits<char>, CmiAlloc<char> > CmiString;

	typedef set<Socket*, less<Socket*>, CmiAlloc<Socket*>> SocketCtxSet;
	typedef list<Packet*, CmiAlloc<Packet*>> PacketList;
	typedef list<Socket*, CmiAlloc<Socket*>> SocketCtxList;
	typedef list<ServerInfo*, CmiAlloc<ServerInfo*>> RemoteServerInfoList;

	//typedef unordered_map<uint64_t, Socket*, hash_compare<uint64_t, less<uint64_t>>, CmiAlloc<pair<uint64_t, Socket*>>> SocketHashMap;
	typedef unordered_map<socket_id_t, Socket*, hash<socket_id_t>, equal_to<socket_id_t>, CmiAlloc<pair<socket_id_t, Socket*>>> SocketHashMap;

	typedef unordered_map<uint64_t, BaseMsgCoroutine*, hash<uint64_t>, equal_to<uint64_t>, CmiAlloc<pair<uint64_t, BaseMsgCoroutine*>>> MsgCoroutineHashMap;
	//typedef unordered_map<int32_t, vector<BaseMsgCoroutine*, CmiAlloc<BaseMsgCoroutine*>>, hash<int32_t>, equal_to<int32_t>, CmiAlloc<pair<int32_t, vector<BaseMsgCoroutine*, CmiAlloc<BaseMsgCoroutine*>>>>> MsgCoroutinesHashMap;

	typedef struct _WSABUF PackBuf;

	struct PackHeader
	{
		uint16_t type;
		uint16_t len;
	};

	enum ExtractState
	{
		ES_PACKET_HEADLEN_NOT_GET,  //包头长度未获取
		ES_PACKET_HEAD_FULL,     //已获取包头信息	
		ES_PACKET_HEAD_NOT_FULL, //包头信息不全
	};

	enum SocketEvent
	{
		EV_SOCKET_OFFLINE,
		EV_SOCKET_PORT_BEUSED,
		EV_SOCKET_CONNECTED,
		EV_SOCKET_ACCEPTED,
		EV_SOCKET_SEND,
		EV_SOCKET_RECV
	};

	enum DataTransMode
	{
		MODE_STREAM,
		MODE_PACK
	};

	typedef void(*UnPackCallBack)(SocketEvent ev, Socket& socketCtx, void* param);
	typedef int32_t(*GetPackDataLenCB)(DePacketor* dePacket, uint8_t* pack, size_t packLen, int32_t* realPackHeadLen);
	typedef void(*SetDataLengthToPackHeadCallBack)(uint8_t* pack, size_t dataSize);

	// 投递的I/O类型
	enum PostIoType
	{
		POST_IO_CONNECT = 0,
		POST_IO_ACCEPT,                     // Accept操作
		POST_IO_RECV,                       // 接收数据
		POST_IO_SEND,                       // 发送数据
		POST_IO_NULL

	};

	enum SocketType
	{
		UNKNOWN_SOCKET,                  //未定SOCKET类型
		LISTENER_SOCKET,                 //本机监听SOCKET
		LISTEN_CLIENT_SOCKET,            //本机与远程客户端之间通连的SOCKET
		CONNECT_SERVER_SOCKET            //本机与远程远程服务器之间已连接的SOCKET
	};

	enum SocketState
	{
		INIT_FAILD,             //初始化失败
		LISTEN_FAILD,           //监听失败
		LISTENING,              //监听所有过来的连接
		CONNECTED_CLIENT,       //已与客户端连接
		CONNECTTING_SERVER,     //与服务器连接中
		CONNECTED_SERVER,       //已连接服务器
		WAIT_REUSED,            //等待重用SOCKET
		NEW_CREATE,             //新生成的SOCKET
		ASSOCIATE_FAILD,        //绑定到完成端口失败
		BIND_FAILD,             //绑定端口失败
		LOCALIP_INVALID,        //无效的本地地址    
		PORT_BEUSED,            //端口被占用
		RESPONSE_TIMEOUT,       //响应超时
		RECV_DATA_TOO_BIG,      //接收的数据过大
		NORMAL_CLOSE,           //正常关闭
	};


	enum ServerMessage
	{
		SMSG_LISTENER_CREATE_FINISH,    //监听器建立完成
		SMSG_ACCEPT_CLIENT,             //接收到一个客户端连接 
		SMSG_REQUEST_CONNECT_SERVER     //请求连接服务器
	};

	// 工作者线程的线程参数
	typedef struct _tagThreadParams_WORKER
	{
		Server* server;                                   // 类指针，用于调用类中的函数
		int         nThreadNo;                                    // 线程编号
	}THREADPARAMS_WORKER, *PTHREADPARAM_WORKER;


	struct ServerTaskState
	{
		int state;
		int clientSize;
	};

	struct TaskData
	{
		Socket* socketCtx;
		void* dataPtr;
	};

	struct ServerMsgTaskData
	{
		ServerMessage msg;
		ServerTaskMgr* serverTaskMgr;
		void* dataPtr;
	};
