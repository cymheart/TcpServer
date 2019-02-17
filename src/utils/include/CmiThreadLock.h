#ifndef _CMITHREADLOCK_H_
#define _CMITHREADLOCK_H_

#include <Windows.h>
#include"utils/include/CmiNewMemory.h"

class DLL_CLASS CmiThreadLock : public CmiNewMemory
{
public:
	CmiThreadLock(void){
		Init();
	}
	~CmiThreadLock(){
		Close();
	}

	void Lock()
	{
		EnterCriticalSection(&m_lock);
	}

	void UnLock()
	{
		LeaveCriticalSection(&m_lock);
	}

	CRITICAL_SECTION GetCriticalSection()
	{
		return m_lock;
	}

	//protected:
private:
CRITICAL_SECTION m_lock;


	void Init(){
		InitializeCriticalSection(&m_lock);
	}
	void Close(){
		DeleteCriticalSection(&m_lock);
	}
};


//自动加锁类
class CmiAutoLock
{
public:
	CmiAutoLock(CmiThreadLock *pThreadLock){
		m_pThreadLock = pThreadLock;
		if (NULL != m_pThreadLock)
		{
			m_pThreadLock->Lock();
		}
	}
	~CmiAutoLock(){
		if (NULL != m_pThreadLock)
		{
			m_pThreadLock->UnLock();
		}
	}
private:
	CmiThreadLock * m_pThreadLock;
};


#endif
