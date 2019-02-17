#include"utils/include/IocpExFuncs.h"

LPFN_CONNECTEX               lpfnConnectEx;
LPFN_DISCONNECTEX            lpfnDisConnectEx;
LPFN_ACCEPTEX                lpfnAcceptEx;               // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
LPFN_GETACCEPTEXSOCKADDRS    lpfnGetAcceptExSockAddrs;


bool LoadIocpExFuncs()
{
	SOCKET tmpsock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (false == LoadWsaFunc_AccpetEx(tmpsock))
	{
		return false;
	}

	if (false == LoadWsaFunc_ConnectEx(tmpsock))
	{
		return false;
	}

	closesocket(tmpsock);
	return true;
}

//本函数利用参数返回函数指针
bool LoadWsaFunc_AccpetEx(SOCKET tmpsock)
{
	DWORD dwBytes = 0;

	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	// 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
	// 所以需要额外获取一下函数的指针，
	// 获取AcceptEx函数指针

	if (!lpfnAcceptEx && SOCKET_ERROR == WSAIoctl(
		tmpsock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&lpfnAcceptEx,
		sizeof(lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "WSAIoctl 未能获取AcceptEx函数指针。错误代码: " << WSAGetLastError());
		return false;
	}

	// 获取GetAcceptExSockAddrs函数指针，也是同理
	if (!lpfnGetAcceptExSockAddrs && SOCKET_ERROR == WSAIoctl(
		tmpsock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs),
		&lpfnGetAcceptExSockAddrs,
		sizeof(lpfnGetAcceptExSockAddrs),
		&dwBytes,
		NULL,
		NULL))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码:" << WSAGetLastError());
		return false;
	}

	return true;
}

bool LoadWsaFunc_ConnectEx(SOCKET tmpsock)
{
	DWORD dwBytes = 0;

	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidConnectEx = WSAID_CONNECTEX;
	GUID GuidDisconnectEx = WSAID_DISCONNECTEX;

	if (!lpfnConnectEx && SOCKET_ERROR == WSAIoctl(
		tmpsock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidConnectEx,
		sizeof(GuidConnectEx),
		&lpfnConnectEx,
		sizeof(lpfnConnectEx),
		&dwBytes,
		NULL,
		NULL))
	{
		//LOG4CPLUS_ERROR(log.GetInst(), "WSAIoctl 未能获取ConnectEx函数指针。错误代码:" << WSAGetLastError());
		return false;
	}

	if (!lpfnDisConnectEx && WSAIoctl(
		tmpsock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidDisconnectEx,
		sizeof(GuidDisconnectEx),
		&lpfnDisConnectEx,
		sizeof(lpfnDisConnectEx),
		&dwBytes,
		NULL,
		NULL))
	{
		return false;
	}

	return true;
}