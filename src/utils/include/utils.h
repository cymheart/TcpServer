#ifndef _UTILS_H_
#define _UTILS_H_

#define WIN32_LEAN_AND_MEAN

#include<windows.h>
#include <IPHlpApi.h> 

#include <assert.h>
#include<string.h>
#include<stdio.h>
#include<stdint.h>
#include <time.h>

#include<winsock2.h>
#include<MSWSock.h>
#include<WS2TCPIP.H>
#include <tcpmib.h>  

#include<io.h>
#include <jemalloc/include/jemalloc/jemalloc.h>

#include<string>
#include <iostream>
#include <random>

using std::cout; using std::endl;
using std::default_random_engine;
using std::uniform_int_distribution;

using namespace std;


#if defined(_WIN32) && !(defined(__GNUC__)  || defined(__GCCXML__)) && !defined(_CMI_LIB) && defined(_CMI_DLL)
#define DLL_CLASS __declspec(dllexport)
#define DLL_FUNC __declspec(dllexport)
#else
#define DLL_CLASS __declspec(dllimport)
#define DLL_FUNC __declspec(dllimport)
#endif




#ifndef MAX
#define MAX(a,b) (a>b)?(a):(b)
#endif

#ifndef MIN
#define MIN(a,b) (a<b)?(a):(b)
#endif


#define EPSINON  0.000001


#define MALLOC(size)                   je_malloc(size);
#define CALLOC(num, size)              je_calloc(num, size);
#define FREE(ptr)                      {if(ptr != nullptr){ je_free(ptr); ptr = nullptr;}}

// 释放指针宏
#define RELEASE(obj)                      {if(obj != nullptr ){delete obj;obj=nullptr;}}

// 释放指针宏
#define RELEASE_REF(obj)                  {if(obj != nullptr ){obj->release(); obj = nullptr;}}

// 释放数组指针宏
#define RELEASE_GROUP(obj)                {if(x != nullptr ){delete [] obj;obj=nullptr;}}

// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}

DLL_FUNC SOCKET TcpWSASocket();

DLL_FUNC int GetNumProcessors();  // 获得本机中处理器的数量							  
DLL_FUNC int get_cpu_usage();     // 获取当前进程的cpu使用率，返回 - 1失败

//void GetLocalIP(char* ip);
//bool CreateAddressInfo(const char* name, u_short hostshort, SOCKADDR_IN* localAddress);

DLL_FUNC void GetLocalIPEx(char* ip);
DLL_FUNC bool CreateAddressInfoEx(const char* name, u_short hostshort, SOCKADDR_IN* localAddress);

//#if _WIN32_WINNT < 0x0600
DLL_FUNC uint64_t CmiGetTickCount64();
//#endif

#ifdef UNICODE
#define odprintf odprintfw
#else
#define odprintf odprintfa
#endif // !UNICODE

struct timezone;

DLL_FUNC void __cdecl odprintfw(const wchar_t *format, ...);
DLL_FUNC void __cdecl odprintfa(const char *format, ...);


DLL_FUNC bool GetTcpPortState(ULONG nPort, ULONG *nStateID);
DLL_CLASS ULONG CreateRandomTcpPort(default_random_engine& e, ULONG startRandomPort, ULONG endRandomPort);


#ifndef _WIN64
//DLL_FUNC uint64_t htonll(uint64_t host);
//DLL_FUNC uint64_t ntohll(uint64_t net);
#endif

#endif



