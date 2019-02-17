#ifndef SINGLETON_DIFINE_H_  
#define SINGLETON_DIFINE_H_  

//µ¥¼þÀàºê  
#define SINGLETON(_CLASS_)                              \
public:                                                 \
	inline static _CLASS_& GetInstance()                \
   {                                                    \
       if (NULL == _instance)                           \
       {                                                \
	        lock.Lock();                                \
	        if (NULL == _instance)                      \
	        {                                           \
				static _CLASS_ inst;                    \
				_instance = &inst;	                    \
	        }                                           \
            lock.UnLock();                              \
       }                                                \
       return *_instance;                               \
   }                                                   \
private:                                                \
	_CLASS_();                                         \
	_CLASS_(_CLASS_ const&) : _CLASS_() {}               \
	_CLASS_& operator= (_CLASS_ const&) { return *this; }  \
	~_CLASS_();                                           \
private:                                                    \
     static CmiThreadLock lock;                             \
	 static _CLASS_* _instance;                           


#define BUILD_SHARE(_CLASS_)\
   _CLASS_* _CLASS_::_instance = NULL; \
   CmiThreadLock _CLASS_::lock;

#endif //SINGLETON_DIFINE_H_  