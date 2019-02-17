#pragma once
#include"utils/include/utils.h"
#include"utils/include/CmiNewMemory.h"

#define MAX_ALIGN_BYTES 4
#define DEFAULT_BUF_SIZE 1024

class ByteStream;

template <typename BaseType> struct Checker;
template <> struct Checker<int8_t> { typedef int8_t Type; };
template <> struct Checker<int16_t> { typedef int16_t Type; };
template <> struct Checker<int32_t> { typedef int32_t Type; };
template <> struct Checker<int64_t> { typedef int64_t Type; };
template <> struct Checker<uint8_t> { typedef uint8_t Type; };
template <> struct Checker<uint16_t> { typedef uint16_t Type; };
template <> struct Checker<uint32_t> { typedef uint32_t Type; };
template <> struct Checker<uint64_t> { typedef uint64_t Type; };
template <> struct Checker<float> { typedef float Type; };
template <> struct Checker<double> { typedef double Type; };
template <> struct Checker<void*> { typedef void* Type; };

#define WCB(wcb, StructType) size_t (*wcb)(ByteStream& writeStream, StructType& value, bool isHostToNet)
#define RCB(rcb, StructType) StructType (*rcb)(ByteStream& readStream, bool isNetToHost)

class ByteStream :public CmiNewMemory
{
public:

	ByteStream(uint8_t* _extrenBuf = nullptr, uint32_t _extrenBufSize = 0)
		:maxAlginBytes(MAX_ALIGN_BYTES)
		, bufSize(DEFAULT_BUF_SIZE)
		, externBuf(_extrenBuf)
		, curtWriteByteNum(0)
		, canReused(false)
		, isError(false)
		, isWriteHostToNet(true)
		, isReadNetToHost(true)
		, isByteAlign(true)
	{
		if (externBuf == nullptr)
		{
			curtBuf = defaultBuf;
		}
		else
		{
			curtBuf = externBuf;
			bufSize = _extrenBufSize;
			curtWriteByteNum = bufSize;
		}

		curtPtr = curtBuf;
	}

	~ByteStream()
	{
		if (canReused)
		{
			if (curtBuf != defaultBuf && curtBuf != externBuf)
				FREE(curtBuf);
		}
	}

	void SetMaxAlginBytes(int alginBytes)
	{
		maxAlginBytes = alginBytes;
	}

	void SetCanReusedMemory(bool canReusedMemory)
	{
		canReused = canReusedMemory;
	}

	void SetWriteHostToNet(bool isWriteHostToNet)
	{
		this->isWriteHostToNet = isWriteHostToNet;
	}

	void SetReadNetToHost(bool isReadNetToHost)
	{
		this->isReadNetToHost = isReadNetToHost;
	}

	void OpenByteAlign(bool open)
	{
		isByteAlign = open;
	}

	//根据设置有可能会拷贝当前buf，或者直接获取当前buf
	uint8_t* TakeBuf()
	{
		if (canReused)
		{
			void* buf = MALLOC(GetNumberOfWriteBytes());
			memcpy(buf, curtBuf, GetNumberOfWriteBytes());
			return (uint8_t*)buf;
		}

		if (curtBuf != defaultBuf)
		{
			return curtBuf;
		}

		void* buf = MALLOC(GetNumberOfWriteBytes());
		memcpy(buf, curtBuf, GetNumberOfWriteBytes());
		return (uint8_t*)buf;
	}

	//直接获取外部buf指针
	uint8_t* GetExternBuf()
	{
		return externBuf;
	}

	//直接获取buf指针
	uint8_t* GetBuf()
	{
		return curtBuf;
	}

	//直接获取buf的当前位置指针
	uint8_t* GetCurt()
	{
		return curtPtr;
	}

	//设置buf的当前位置指针
	uint8_t* SetCurt(int32_t curtPtrPos)
	{
		if (curtPtrPos > curtWriteByteNum)
			curtPtrPos = curtWriteByteNum;
		else if (curtPtrPos < 0)
			curtPtrPos = 0;

		curtPtr = curtBuf + curtPtrPos;
		return curtPtr;
	}

	//设置buf的当前位置指针到后面位置
	uint8_t* Next(int32_t pos)
	{
		return SetCurt(curtPtr + pos - curtBuf);
	}

	//获取buf的总大小
	size_t GetBufSize()
	{
		return bufSize;
	}

	//获取总的写入字节数
	size_t GetNumberOfWriteBytes()
	{
		return curtWriteByteNum;
	}

	//获取当前位置的前向字节数
	size_t GetNumberOfCurtBytes()
	{
		return curtPtr - curtBuf;
	}

	//获取当前位置后剩余已写入的字节数
	size_t GetNumberOfRichBytes()
	{
		return curtWriteByteNum - (curtPtr - curtBuf);
	}

	//重设外部buf
	void ResetExtrenBuf(uint8_t* _extrenBuf, uint32_t _extrenBufSize)
	{
		Clear();
		externBuf = _extrenBuf;
		curtPtr = curtBuf = externBuf;
		bufSize = _extrenBufSize;
		curtWriteByteNum = bufSize;
	}

	void Clear()
	{
		isError = false;
		curtWriteByteNum = 0;
		curtPtr = curtBuf;
	}

	void MemsetCurtRichBytes(int32_t byteNum, int32_t val)
	{
		int richlen = curtPtr + byteNum - curtBuf - bufSize;
		if (richlen > 0) {
			if (TestWriteBufSize(curtPtr + richlen) != 0)
				return;
			byteNum -= richlen;
		}

		memset(curtPtr, val, byteNum);
	}

	int TestWriteBufSize(uint8_t* testPtr)
	{
		if (bufSize == 0)
			return 0;

		if (testPtr >= curtBuf + bufSize)
		{
			if (externBuf != nullptr)
				return 1;

			size_t allocSize = bufSize * 2;

			if (allocSize < (size_t)(testPtr - curtBuf))
			{
				allocSize = (testPtr - curtBuf) * 2;
			}

			int curtPtrOffsetBytes = (int)(curtPtr - curtBuf);
			uint8_t* newBuf = (uint8_t*)MALLOC(allocSize);
			memcpy(newBuf, curtBuf, bufSize);


			if (curtBuf != defaultBuf)
				FREE(curtBuf);

			curtBuf = newBuf;
			bufSize = allocSize;
			curtPtr = curtBuf + curtPtrOffsetBytes;

			return 0;
		}

		return 0;
	}

	int AlignWriteAddr(int structBytes, int saveDataSize = 0, bool isTestWriteBuf = false)
	{
		int ret = 0;
		int offsetBytes = 0;
		int alginBytes = 0;

		if (isByteAlign)
		{
			if (structBytes > maxAlginBytes)
				alginBytes = maxAlginBytes;
			else
				alginBytes = structBytes;

			int n = (int)(curtPtr - curtBuf) % alginBytes;
			if (n != 0)
				offsetBytes = alginBytes - n;
		}

		if (isTestWriteBuf) {
			ret = TestWriteBufSize(curtPtr + offsetBytes + saveDataSize);
			if (ret != 0)
				return ret;
		}

		curtPtr += offsetBytes;
		return ret;
	}

	template<typename BaseType>
	typename Checker<BaseType>::Type
		Write(BaseType val)
	{
		if (AlignWriteAddr(sizeof(BaseType), sizeof(BaseType), true) != 0)
			return val;

		SetBaseTypeValue(val);
		curtPtr += sizeof(BaseType);

		if (curtPtr - curtBuf > curtWriteByteNum)
			curtWriteByteNum = curtPtr - curtBuf;

		return val;
	}


	template<typename BaseType>
	typename Checker<BaseType>::Type
		Write(BaseType* val)
	{
		if (AlignWriteAddr(sizeof(BaseType), sizeof(BaseType), true) != 0)
			return *val;

		SetBaseTypeValue(val);
		curtPtr += sizeof(BaseType);

		if (curtPtr - curtBuf > curtWriteByteNum)
			curtWriteByteNum = curtPtr - curtBuf;

		return *val;
	}

	template<typename BaseType>
	void SetBaseTypeValue(BaseType val)
	{
		if (!isWriteHostToNet) {
			*((BaseType*)curtPtr) = val;
			return;
		}

		switch (sizeof(BaseType))
		{
		case 2:
		{
			uint16_t newVal = htons(*((uint16_t*)&val));
			*((BaseType*)curtPtr) = (BaseType)newVal;
		}
		break;

		case 4:
		{
			uint32_t newVal = htonl(*((uint32_t*)&val));
			*((BaseType*)curtPtr) = (BaseType)newVal;
		}
		break;

		case 8:
		{
			uint64_t newVal = htonll(*((uint64_t*)&val));
			*((BaseType*)curtPtr) = (BaseType)newVal;
		}
		break;

		default:
			*((BaseType*)curtPtr) = val;
		}
	}


	void WriteBytes(void* inByteArray, uint32_t numberOfBytesToWrite)
	{
		int offsetBytes = 0;

		if (isByteAlign)
		{
			int n = (int)(curtPtr - curtBuf) % maxAlginBytes;
			if (n != 0) { offsetBytes = maxAlginBytes - n; }
		}

		if (TestWriteBufSize(curtPtr + offsetBytes + numberOfBytesToWrite) != 0)
			return;

		curtPtr += offsetBytes;

		memcpy(curtPtr, inByteArray, numberOfBytesToWrite);
		curtPtr += numberOfBytesToWrite;
		WriteAlignBytes();
	}


	template<typename StructType, WCB(wcb, StructType)>
	size_t WriteStruct(StructType& val)
	{
		return wcb(*this, val, isWriteHostToNet);
	}

	template<typename StructType, WCB(wcb, StructType)>
	size_t WriteStruct(StructType* val)
	{
		return wcb(*this, *val, isWriteHostToNet);
	}


	void WriteAlignBytes()
	{
		int offsetBytes = 0;

		if (isByteAlign)
		{
			int n = (int)(curtPtr - curtBuf) % maxAlginBytes;
			if (n != 0) { offsetBytes = maxAlginBytes - n; }

			if (TestWriteBufSize(curtPtr + offsetBytes) != 0)
				return;
		}

		curtPtr += offsetBytes;

		if (curtPtr - curtBuf  > curtWriteByteNum)
			curtWriteByteNum = curtPtr - curtBuf;
	}

	void WriteByteBits(uint8_t bitsValue, int startBit, int endBit, bool toNextByte = false)
	{
		if (startBit < 0 || startBit > 7 ||
			endBit < 0 || endBit > 7 || startBit > endBit)
			return;
	
		if (TestWriteBufSize(curtPtr) != 0)		
			return;
	
		uint8_t byteValue = (bitsValue << startBit) & (0xff >> (7 - endBit));
		*curtPtr |= byteValue;

		if (curtPtr - curtBuf > curtWriteByteNum)
			curtWriteByteNum = curtPtr - curtBuf;

		if (toNextByte)
			curtPtr++;
	}


	void AlignReadAddr(int structBytes)
	{
		if (!isByteAlign)
			return;

		int alginBytes;
		if (structBytes > maxAlginBytes)
			alginBytes = maxAlginBytes;
		else
			alginBytes = structBytes;

		int n = (int)(curtPtr - curtBuf) % alginBytes;
		int offsetBytes = 0;
		if (n != 0)
			offsetBytes = alginBytes - n;

		curtPtr += offsetBytes;
	}


	template<typename BaseType>
	typename Checker<BaseType>::Type
		Read(BaseType& val)
	{
		if (isError) {
			val = 0;
			return val;
		}

		AlignReadAddr(sizeof(BaseType));
		val = GetBaseTypeValue<BaseType>();
		curtPtr += sizeof(BaseType);
		return val;
	}

	template<typename BaseType>
	typename Checker<BaseType>::Type
		Read(BaseType* valPtr)
	{
		if (isError) {
			*valPtr = 0;
			return *valPtr;
		}

		AlignReadAddr(sizeof(BaseType));
		*valPtr = GetBaseTypeValue<BaseType>();
		curtPtr += sizeof(BaseType);

		return *valPtr;
	}

	template<typename BaseType>
	typename Checker<BaseType>::Type
		Read()
	{
		if (isError) {
			return 0;
		}

		AlignReadAddr(sizeof(BaseType));
		BaseType val = GetBaseTypeValue<BaseType>();
		curtPtr += sizeof(BaseType);
		return val;
	}

	template<typename BaseType>
	BaseType GetBaseTypeValue()
	{
		if (bufSize != 0 &&
			curtPtr + sizeof(BaseType) - curtBuf > bufSize)
		{
			isError = true;
			return 0;
		}

		if (!isReadNetToHost)
		{
			return *(BaseType*)curtPtr;
		}

		switch (sizeof(BaseType))
		{
		case 2:
		{

			uint16_t newVal = ntohs(*(uint16_t*)curtPtr);
			return (BaseType)newVal;
		}
		break;

		case 4:
		{
			uint32_t newVal = ntohl(*(uint32_t*)curtPtr);
			return (BaseType)newVal;
		}
		break;

		case 8:
		{
			uint64_t newVal = ntohll(*(uint64_t*)curtPtr);
			return (BaseType)newVal;
		}
		break;

		default:
			return *(BaseType*)curtPtr;
		}
	}

	uint8_t* ReadBytesPointer(uint32_t readBytes = 0)
	{
		if (isError) {
			return nullptr;
		}

		ReadAlignBytes();
		uint8_t* ret = curtPtr;
		curtPtr += readBytes;
		ReadAlignBytes();

		if (bufSize != 0 &&
			curtPtr - curtBuf > bufSize)
		{
			isError = true;
			return nullptr;
		}

		return ret;
	}

	template<typename StructType, RCB(rcb, StructType)>
	void ReadStruct(StructType& val)
	{
		val = rcb(*this, isReadNetToHost);
	}

	template<typename StructType, RCB(rcb, StructType)>
	void ReadStruct(StructType* val)
	{
		*val = rcb(*this, isReadNetToHost);
	}

	template<typename StructType, RCB(rcb, StructType)>
	StructType ReadStruct()
	{
		return rcb(*this, isReadNetToHost);
	}

	void ReadAlignBytes()
	{
		if (!isByteAlign)
			return;

		int n = (int)(curtPtr - curtBuf) % maxAlginBytes;
		int offsetBytes = 0;
		if (n != 0) { offsetBytes = maxAlginBytes - n; }
		curtPtr += offsetBytes;
	}


	//读取字节位值
	uint8_t ReadByteBits(int startBit, int endBit, bool toNextByte = false)
	{
		if (bufSize != 0 &&
			curtPtr - curtBuf > bufSize)
		{
			isError = true;
			return 0;
		}

		if (startBit < 0 || startBit > 7 || 
			endBit < 0 || endBit > 7 || startBit > endBit)
			return 0;

		uint8_t byteValue = *curtPtr;
		int shr = startBit;
		uint8_t bitsValue = (byteValue & (0xff >> (7 - endBit))) >> shr;

		if (toNextByte)
			curtPtr++;

		return bitsValue;
	}


	template<typename BaseType>
	typename Checker<BaseType>::Type
		Igrone()
	{
		return Read<BaseType>();
	}

	void IgroneBytes(uint32_t readBytes = 0)
	{
		ReadBytesPointer(readBytes);
	}

	void IgroneAlignBytes()
	{
		ReadAlignBytes();
	}

private:
	bool canReused;
	bool isWriteHostToNet;
	bool isReadNetToHost;
	bool isError;
	bool isByteAlign;
	int maxAlginBytes;
	uint8_t defaultBuf[DEFAULT_BUF_SIZE];
	size_t bufSize;
	size_t curtWriteByteNum;
	uint8_t* externBuf;
	uint8_t* curtBuf;
	uint8_t* curtPtr;
};