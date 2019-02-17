#include"utils/include/utils.h"

DLL_FUNC SOCKET TcpWSASocket()
{
	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	// 为以后新连入的客户端先准备好Socket
	return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

//#if _WIN32_WINNT < 0x0600 
DLL_FUNC uint64_t CmiGetTickCount64()
{
	uint64_t Ret;
	static LARGE_INTEGER TicksPerSecond = { 0 };
	LARGE_INTEGER Tick;

	if (!TicksPerSecond.QuadPart)
	{
		QueryPerformanceFrequency(&TicksPerSecond);
	}

	QueryPerformanceCounter(&Tick);

	LONGLONG Seconds = Tick.QuadPart / TicksPerSecond.QuadPart;
	LONGLONG LeftPart = Tick.QuadPart - (TicksPerSecond.QuadPart*Seconds);
	LONGLONG MillSeconds = LeftPart * 1000 / TicksPerSecond.QuadPart;
	Ret = Seconds * 1000 + MillSeconds;

	return Ret;
}
//#endif

DLL_FUNC void __cdecl odprintfw(const wchar_t *format, ...)
{
	wchar_t buf[4096], *p = buf;
	va_list args;
	va_start(args, format);
	p += _vsnwprintf(p, sizeof(buf) / sizeof(wchar_t), format, args);
	va_end(args);
	OutputDebugStringW(buf);
}

DLL_FUNC void __cdecl odprintfa(const char *format, ...)
{
	char buf[4096], *p = buf;
	va_list args;
	va_start(args, format);
	p += _vsnprintf(p, sizeof(buf)-1, format, args);
	va_end(args);

	OutputDebugStringA(buf);
}


DLL_FUNC int GetNumProcessors()
{        
	SYSTEM_INFO si;
	GetSystemInfo(&si);      
	return si.dwNumberOfProcessors;
}

//// 获得本机的IP地址
//void GetLocalIP(char* ip)
//{
//	// 获得本机主机名
//	char hostname[MAX_PATH] = { 0 };
//
//	gethostname(hostname, MAX_PATH);
//
//	hostent* lpHostEnt = (hostent*)gethostbyname(hostname);
//
//	if (lpHostEnt == NULL)
//	{
//		return;
//	}
//
//	// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
//	char* lpAddr = lpHostEnt->h_addr_list[0];
//
//	// 将IP地址转化成字符串形式
//	struct in_addr inAddr;
//	memmove(&inAddr, lpAddr, 4);
//	char* ips = inet_ntoa(inAddr);
//
//	for (int i = 0; ips[i] != '\0'; i++)
//		ip[i] = ips[i];
//}
//
//bool CreateAddressInfo(const char* name, u_short hostshort, SOCKADDR_IN* sockAddress)
//{
//	// 生成本机绑定地址信息
//	HOSTENT *local;
//
//	local = (hostent*)gethostbyname(name);
//	if (local == NULL)
//		return false;
//
//	memset(sockAddress, 0, sizeof(sockAddress));
//	sockAddress->sin_family = AF_INET;
//	memcpy((char *)sockAddress->sin_addr.s_addr, (char *)local->h_addr, local->h_length);
//	sockAddress->sin_port = htons(hostshort);
//
//	return true;
//}

DLL_FUNC void GetLocalIPEx(char* ip)
{
	ADDRINFOW *result = NULL;
	ADDRINFOW *ptr = NULL;
	ADDRINFOW hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// 获得本机主机名
	char hostname[MAX_PATH] = { 0 };
	gethostname(hostname, MAX_PATH);
	SOCKADDR_IN* sockAddress = (SOCKADDR_IN*)MALLOC(sizeof(SOCKADDR_IN));
	CreateAddressInfoEx(hostname, 0, sockAddress);
	
	inet_ntop(AF_INET, (void *)&(sockAddress->sin_addr), ip, 16);

	FREE(sockAddress);
}

DLL_FUNC bool CreateAddressInfoEx(const char* name, u_short hostshort, SOCKADDR_IN* sockAddress)
{
	ADDRINFOW *result = NULL;
	ADDRINFOW *ptr = NULL;
	ADDRINFOW hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// 获得本机主机名
	wchar_t hostname[MAX_PATH] = { 0 };
	size_t len = MultiByteToWideChar(CP_ACP, 0, name, (int)strlen(name), NULL, 0);
	MultiByteToWideChar(CP_ACP, 0, name, (int)strlen(name), hostname, (int)len);

	int dwRetval = GetAddrInfo(hostname, 0, &hints, &result);
	if (dwRetval != 0)
	{
		return false;
	}
	else
	{
	//	LPSOCKADDR sockaddr_ip;
	//	DWORD ipbufferlength;
		int i = 0;
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
		{

			//printf("getaddrinfo response %d\n", i++);
			//printf("\tFlags: 0x%x\n", ptr->ai_flags);
			//printf("\tFamily: ");
			switch (ptr->ai_family)
			{
			case AF_UNSPEC:
				printf("Unspecified\n");
				break;

			case AF_INET:
				//printf("AF_INET (IPv4)\n");
				memcpy(sockAddress, ptr->ai_addr, sizeof(SOCKADDR_IN));
				sockAddress->sin_port = htons(hostshort);
				goto end;

			case AF_INET6:
				//printf("AF_INET6 (IPv6)\n");

				//// the InetNtop function is available on Windows Vista and later
				//// sockaddr_ipv6 = (struct sockaddr_in6 *) ptr->ai_addr;
				//// printf("\tIPv6 address %s\n",
				////    InetNtop(AF_INET6, &sockaddr_ipv6->sin6_addr, ipstringbuffer, 46) );

				//// We use WSAAddressToString since it is supported on Windows XP and later
				//sockaddr_ip = (LPSOCKADDR)ptr->ai_addr;

				//// The buffer length is changed by each call to WSAAddresstoString
				//// So we need to set it for each iteration through the loop for safety
				//ipbufferlength = 46;

				//int iRetval = WSAAddressToString(sockaddr_ip, (DWORD)ptr->ai_addrlen, NULL,
				//	ipstringbuffer, &ipbufferlength);

				//if (iRetval)
				//	printf("WSAAddressToString failed with %u\n", WSAGetLastError());
				//else
				//	printf("\tIPv6 address %s\n", ipstringbuffer);
				break;

			case AF_NETBIOS:
				printf("AF_NETBIOS (NetBIOS)\n");
				break;

			default:
				printf("Other %ld\n", ptr->ai_family);
				break;
			}
		}

end:
		FreeAddrInfo(result);
		return true;
	}
}


//获取Tcp端口状态  
DLL_FUNC bool GetTcpPortState(ULONG nPort, ULONG *nStateID)
{
	MIB_TCPTABLE TcpTable[100];
	DWORD nSize = sizeof(TcpTable);
	if (NO_ERROR == GetTcpTable(&TcpTable[0], &nSize, TRUE))
	{
		DWORD nCount = TcpTable[0].dwNumEntries;
		if (nCount > 0)
		{
			for (DWORD i = 0; i<nCount; i++)
			{
				MIB_TCPROW TcpRow = TcpTable[0].table[i];
				DWORD temp1 = TcpRow.dwLocalPort;
				int temp2 = temp1 / 256 + (temp1 % 256) * 256;
				if (temp2 == nPort)
				{
					*nStateID = TcpRow.dwState;
					return true;
				}
			}
		}
		return false;
	}
	return false;
}

//
DLL_CLASS ULONG CreateRandomTcpPort(default_random_engine& e, ULONG startRandomPort, ULONG endRandomPort)
{
	uniform_int_distribution<unsigned> u(startRandomPort, endRandomPort); //随机数分布对象 
	ULONG port = u(e);

	ULONG nStateID;
	int faildCount = 0;

	while (GetTcpPortState(port, &nStateID))
	{
		faildCount++;

		if (faildCount > 10)
			return 0;

		port = u(e);
	}
	return port;
}

/// 时间转换  
uint64_t file_time_2_utc(const FILETIME* ftime)
{
	LARGE_INTEGER li;

	assert(ftime);
	li.LowPart = ftime->dwLowDateTime;
	li.HighPart = ftime->dwHighDateTime;
	return li.QuadPart;
}

DLL_FUNC int get_cpu_usage()
{
	//cpu数量  
	static int processor_count_ = -1;
	//上一次的时间  
	static int64_t last_time_ = 0;
	static int64_t last_system_time_ = 0;


	FILETIME now;
	FILETIME creation_time;
	FILETIME exit_time;
	FILETIME kernel_time;
	FILETIME user_time;
	int64_t system_time;
	int64_t time;
	int64_t system_time_delta;
	int64_t time_delta;

	int cpu = -1;


	if (processor_count_ == -1)
	{
		processor_count_ = GetNumProcessors();
	}

	GetSystemTimeAsFileTime(&now);

	if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
		&kernel_time, &user_time))
	{
		// We don't assert here because in some cases (such as in the Task Manager)
		// we may call this function on a process that has just exited but we have
		// not yet received the notification.  
			return -1;
	}

	system_time = (file_time_2_utc(&kernel_time) + file_time_2_utc(&user_time)) / processor_count_;
	time = file_time_2_utc(&now);

	if ((last_system_time_ == 0) || (last_time_ == 0))
	{
		// First call, just set the last values.  
		last_system_time_ = system_time;
		last_time_ = time;
		return -1;
	}

	system_time_delta = system_time - last_system_time_;
	time_delta = time - last_time_;

	assert(time_delta != 0);

	if (time_delta == 0)
		return -1;

	// We add time_delta / 2 so the result is rounded.  
	cpu = (int)((system_time_delta * 100 + time_delta / 2) / time_delta);
	last_system_time_ = system_time;
	last_time_ = time;
	return cpu;
}



#ifndef _WIN64
//uint64_t htonll(uint64_t host)
//{
//	int i = 1;
//	char j = *((char*)&i);
//	if (j == 1)
//		return host;
//
//	uint64_t ret = 0;
//	uint32_t  high, low;
//
//	low = host & 0xFFFFFFFF;
//	high = (host >> 32) & 0xFFFFFFFF;
//	low = htonl(low);
//	high = htonl(high);
//
//	ret = low;
//	ret <<= 32;
//	ret |= high;
//	return   ret;
//}
//
//
//uint64_t ntohll(uint64_t net)
//{
//	int i = 1;
//	char j = *((char*)&i);
//	if (j == 1)
//		return net;
//
//	uint64_t   ret = 0;
//	uint32_t   high, low;
//
//	low = net & 0xFFFFFFFF;
//	high = (net >> 32) & 0xFFFFFFFF;
//	low = ntohl(low);
//	high = ntohl(high);
//
//	ret = low;
//	ret <<= 32;
//	ret |= high;
//	return   ret;
//}
#endif