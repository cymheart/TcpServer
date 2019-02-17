/*
ID 生成策略
毫秒级时间41位+机器ID 10位+毫秒内序列12位。
0 41 51 64 +-----------+------+------+ |time |pc |inc | +-----------+------+------+
前41bits是以毫秒为单位的timestamp。
接着10bits是事先配置好的机器ID。
最后12bits是累加计数器。
macheine id(10bits)标明最多只能有1024台机器同时产生ID，sequence number(12bits)也标明1台机器1ms中最多产生4096个ID， *
注意点，因为使用到位移运算，所以需要64位操作系统，不然生成的ID会有可能不正确
*/
#pragma once

#include"utils/include/utils.h"
#include <time.h>
#include "utils/include/CmiThreadLock.h"
#include"utils/include/Singleton.h"

class DLL_CLASS UniqueID
{
	SINGLETON(UniqueID)

public:
	void set_workid(int workid);
	uint64_t gen();
	uint64_t UniqueID::gen_multi();

private:
	uint64_t get_curr_ms();
	uint64_t wait_next_ms(uint64_t lastStamp);

private:
	CmiThreadLock threadLock;
	uint64_t last_stamp;
	int workid;
	volatile uint64_t seqid;
};

