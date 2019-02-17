#ifndef _CMINEWMEMORY_H_
#define _CMINEWMEMORY_H_

#include"utils/include/utils.h"

class DLL_CLASS CmiNewMemory
{
public:

	CmiNewMemory() {}
	virtual ~CmiNewMemory() {}

	
	void* operator new(size_t size)
	{
		return MALLOC(size);
	}

	void operator delete(void *p)
	{
		FREE(p);
	}


	void* operator new[](size_t size)
	{
		return MALLOC(size);
	}
	
	void operator delete[](void *p)
	{
		FREE(p);
	}
};


class DLL_CLASS CmiNewMemoryRef
{
public:

	CmiNewMemoryRef()
		:ref(1)
	{
	}
	virtual ~CmiNewMemoryRef() {}

	void addRef() { ref++; }
	int32_t getRef() { return ref; }

	void release()
	{
		ref--;
		if (ref == 0) {
			delete this;
		}
	}

	void* operator new(size_t size)
	{
		return MALLOC(size);
	}


	void operator delete(void *p)
	{
		if (--((CmiNewMemoryRef*)p)->ref <= 0)
			FREE(p);
	}

	void* operator new[](size_t size) {return 0; }
		void operator delete[](void *p) {}

private:
	int32_t ref;
};


#endif