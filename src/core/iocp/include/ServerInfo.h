#pragma once

#include"core/iocp/include/ServerBaseTypes.h"

//已连接的远程服务器
class DLL_CLASS ServerInfo :public CmiNewMemory
{
public:
	ServerInfo();
	~ServerInfo();

	void Copy(ServerInfo& obj);

	void SetServerIP(char* ip)
	{
		memcpy(serverIP, ip, strlen(ip) + 1);
	}

	void SetServerPort(int port)
	{
		serverPort = port;
	}

	char* GetServerIP()
	{
		return serverIP;
	}

	int GetServerPort()
	{
		return serverPort;
	}


	void SetLocalConnectPort(int port)
	{
		localConnectPort = port;
	}

	void SetDePacketor(DePacketor*  _dePacketor);

	void SetDataTransMode(DataTransMode mode)
	{
		dataTransMode = mode;
	}

	Socket* GetSocket()
	{
		return socketCtx;
	}

	int GetLocalConnectPort()
	{
		return localConnectPort;
	}

	DePacketor*  GetDePacketor()
	{
		return dePacketor;
	}

	void SetName(char* name);

	char* GetName()
	{
		return name;
	}

	uint64_t GetTag()
	{
		return tag;
	}

	void SetTag(uint64_t tag)
	{
		this->tag = tag;
	}

private:
	char*                        name;
	char                         serverIP[16];             // 远程服务器IP地址
	int                          serverPort;               // 远程服务器的端口
	int                          localConnectPort;           // 连接远程服务器的端口
	Socket*                      socketCtx;                  // 连接远程服务器的Context信息(此变量存储与远程服务器的连接信息)
	DePacketor*                  dePacketor;
	ServerTask*                  serverTask;
	DataTransMode                dataTransMode;
	uint64_t                     tag;

	friend class Socket;
	friend class Server;
	friend class ServerTask;
};
