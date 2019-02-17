#pragma once

#include"utils/include/utils.h"

class DLL_CLASS CResGuard {

public:
	CResGuard() { InitializeCriticalSection(&m_lock); }
	~CResGuard() { DeleteCriticalSection(&m_lock); }

	void Guard() { EnterCriticalSection(&m_lock); }
	void Unguard() { LeaveCriticalSection(&m_lock); }

private:
	CRITICAL_SECTION m_lock; 
};

template <class T>
class  SingletonTpl
{
public:
	static inline T& instance();

protected:
	SingletonTpl(void) {}
	~SingletonTpl(void) {}

	Singleton(const Singleton&) {}
	Singleton& operator= (const Singleton &) {}

private:
	// This is important  
	class DLL_CLASS GC // 垃圾回收类  
	{
	public:
		GC()
		{
			cout << "GC construction" << endl;
		}
		~GC()
		{
			//cout << "GC destruction" << endl;
			// We can destory all the resouce here, eg:db connector, file handle and so on  
			if (_instance != NULL)
			{
				delete _instance;
				_instance = NULL;
				//cout << "Singleton destruction" << endl;
				//system("pause");//不暂停程序会自动退出，看不清输出信息  
			}
		}
	};
	
	static GC gc;  //垃圾回收类的静态成员  
	static T* _instance;
	static CResGuard _rs;

};

template <class T>
T* SingletonTpl<T>::_instance;

template <class T>
CResGuard SingletonTpl<T>::_rs;

template <class T>
typename SingletonTpl<T>::GC SingletonTpl<T>::gc;

template <class T>
inline T& SingletonTpl<T>::instance()
{

	if (0 == _instance)
	{
		_rs.Guard();
		if (0 == _instance)
		{
			_instance = new T;
		}
		_rs.Unguard();

	}

	return *_instance;

}