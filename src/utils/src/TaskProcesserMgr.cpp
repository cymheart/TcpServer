#include"utils/include/TaskProcesserMgr.h"

TaskProcesserMgr::TaskProcesserMgr(int _processerAmount, TaskProcesser* _statisticsProcesser)
	:processerAmount(_processerAmount)
	, statisticsProcesser(_statisticsProcesser)
{
	CommonTaskProcesser* processer;
	for (int i = 0; i < processerAmount; i++)
	{
		processer = new CommonTaskProcesser();
		processers.push_back(processer);
		processingTaskCount.push_back(0);
		processer->Start();
	}
}

TaskProcesserMgr::~TaskProcesserMgr()
{
	for (int i = 0; i < processerAmount; i++)
	{
		processers[i]->Stop();
		RELEASE(processers[i]);
	}
}

int TaskProcesserMgr::AssignProcessserIdx()
{
	int minAmountIdx = 0;
	for (int i = 1; i < processerAmount; i++)
	{
		if (processingTaskCount[minAmountIdx] > processingTaskCount[i])
			minAmountIdx = i;
	}

	return minAmountIdx;
}


void* TaskProcesserMgr::ProcesserTask(void* data)
{
	TaskProcesserMgr* mgr;
	uint32_t idx;
	TaskCallBack taskCB;
	void* taskData;

	ByteStream dataStream((uint8_t*)data);
	dataStream.SetReadNetToHost(false);

	dataStream.Read((void**)&mgr);
	dataStream.Read(idx);
	dataStream.Read((void**)&taskCB);
	dataStream.Read(taskData);


	taskCB(taskData);

	TaskProcesser* statisticsProcesser = mgr->statisticsProcesser;

	if (statisticsProcesser != nullptr) {
		ByteStream dataStream;
		dataStream.SetWriteHostToNet(false);
		dataStream.Write((void*)mgr);
		dataStream.Write(idx);
		statisticsProcesser->PostTask(DecProcessingAmountTask, NULL, dataStream.TakeBuf());
	}

	FREE(data);
	return 0;
}


void* TaskProcesserMgr::DecProcessingAmountTask(void* data)
{
	TaskProcesserMgr* mgr;
	uint32_t idx;

	ByteStream dataStream((uint8_t*)data);
	dataStream.SetReadNetToHost(false);
	dataStream.Read((void**)&mgr);
	dataStream.Read(idx);

	mgr->processingTaskCount[idx]--;

	FREE(data);
	return 0;
}