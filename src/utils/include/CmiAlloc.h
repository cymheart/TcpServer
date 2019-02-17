#ifndef _CMIALLOC_H_
#define _CMIALLOC_H_

#include"utils/include/utils.h"

template <typename T>
class CmiAlloc : public allocator<T>
{
public:
	typedef size_t   size_type;
	typedef typename allocator<T>::pointer              pointer;
	typedef typename allocator<T>::value_type           value_type;
	typedef typename allocator<T>::const_pointer        const_pointer;
	typedef typename allocator<T>::reference            reference;
	typedef typename allocator<T>::const_reference      const_reference;

	pointer allocate(size_type _Count, const void* _Hint = NULL)
	{
		_Count *= sizeof(value_type);
		void *rtn = MALLOC(_Count);  
		return (pointer)rtn;
	}

	void deallocate(pointer _Ptr, size_type _Count)
	{
		FREE(_Ptr);  
	}

	template<class _Other>
	struct rebind
	{   // convert this type to allocator<_Other>  
		typedef CmiAlloc<_Other> other;
	};

	CmiAlloc() throw()
	{}

	CmiAlloc(const CmiAlloc& __a) throw()
		: allocator<T>(__a)
	{}

	template<typename _Tp1>
	CmiAlloc(const CmiAlloc<_Tp1>&) throw()
	{}

	~CmiAlloc() throw()
	{}
};

#endif