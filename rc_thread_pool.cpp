#include "rc_thread_pool.h"
#include <unistd.h>
#include <algorithm>
#include <iostream>

RCThread::RCThread():ThreadName(""),ThreadID(0)/*,retCode=0*/{}

void RCThread::Create(pthread_attr_t *attr,void *(*handler)(void*),void *arg)
{
	pthread_create(&ThreadID,attr,handler,arg);
	ThreadName.append("default");
}

void RCThread::Join()
{
	pthread_join(ThreadID,NULL);
}


RCThread::~RCThread()
{
	if(ThreadID!=0)
		Join();
}





RCThreadPool::RCThreadPool():m_InitNum(10),idlesize(10),busysize(0){}


RCThreadPool::RCThreadPool(int maxsize):m_InitNum(maxsize),idlesize(maxsize),busysize(0){}

// create guard threads (which monitor the thread pool and recyle the leisure thread)
void RCThreadPool::InitGuard()
{
	RCWorkerThread *temp=new RCWorkerThread(*(new RCThreadCond()),*(new RCThreadMutex()));
	RCJob *temp_job=new RCJob((void*)this);
	temp_job->handler=&GuardThread;
	temp->setJob(*temp_job);
	temp->Prepare();
	temp->Start();
}

// implementation of the guard thread
void *RCThreadPool::GuardThread(void *argv)
{
	RCThreadPool *pool=(RCThreadPool*)argv;
	while(1)
	{
		sleep(5);  // scan the busy list every 5 seconds
		pool->BusyMutex.lock();
		if(pool->BusyList.size())
		{
			if(pool->BusyList.front()->getState()!=RUNNING)
			{
				pool->AppendToIdleList(pool->BusyList.front());
				pool->BusyList.pop_front();
				pool->busysize--;
			}
		}
		pool->BusyMutex.unlock();
	}
}

int RCThreadPool::getIdleSize() 
{
	IdleMutex.lock();
	int size=idlesize;
	IdleMutex.unlock();
	return size;
}

int RCThreadPool::getBusySize() 
{
	BusyMutex.lock();
	int size=busysize;
	BusyMutex.unlock();
	return size;
}

// create init_num thread
void RCThreadPool::CreateThreadPool()
{
	for(int i=0;i<m_InitNum;i++)
	{
		RCThreadCond *temp_cond=new RCThreadCond();
		RCThreadMutex *temp_mux=new RCThreadMutex();
		RCWorkerThread *temp=new RCWorkerThread(*temp_cond,*temp_mux);
		IdleMutex.lock();
		IdleList.push_back(temp);
		IdleMutex.unlock();
		temp->Prepare();
	}
}

void RCThreadPool::AppendToIdleList(RCWorkerThread* t)
{
	IdleMutex.lock();
	if(std::find(IdleList.begin(),IdleList.end(),t)==IdleList.end())
	{
		IdleList.push_back(t);
		t->Suspend();
		idlesize++;
	}
	IdleMutex.unlock();
}

void RCThreadPool::MoveToBusyList(RCWorkerThread* t)
{
	BusyMutex.lock();
	if(std::find(BusyList.begin(),BusyList.end(),t)==BusyList.end())
	{
		BusyList.push_back(t);
		t->Start();
		busysize++;
	}
	BusyMutex.unlock();
}

// add new job to the thread_pool
void RCThreadPool::AddJob(RCJob& this_job)
{
	IdleMutex.lock();
	if(IdleList.size())
	{
		IdleList.front()->setJob(this_job);
		MoveToBusyList(IdleList.front());
		IdleList.pop_front();
		idlesize--;
	}
	printf("job added\n");
	IdleMutex.unlock();
}








RCThreadMutex::RCThreadMutex()
{
	pthread_mutex_init(&mutex,NULL);
}

RCThreadMutex::~RCThreadMutex()
{
  	pthread_mutex_destroy(&mutex);
}

void RCThreadMutex::lock()
{
	pthread_mutex_lock(&mutex);  
}

void RCThreadMutex::unlock()
{
	pthread_mutex_unlock(&mutex);  
}


RCThreadCond::RCThreadCond()
{
	pthread_cond_init(&cond,NULL);
}

RCThreadCond::~RCThreadCond()
{
	pthread_cond_destroy(&cond);
}

void RCThreadCond::signal()
{
	pthread_cond_signal(&cond); 
}

void RCThreadCond::wait(RCThreadMutex& mux)
{
	pthread_cond_wait(&cond, &mux.mutex);
}



RCWorkerThread::RCWorkerThread(){}

RCWorkerThread::RCWorkerThread(RCThreadCond& cond,RCThreadMutex& mux):state(NOTSTART),job(NULL)
{
	this->_cond=&cond;
	this->_mux=&mux;
}

void RCWorkerThread::setJob(RCJob& this_job)
{
	this->job=&this_job;
}

void RCWorkerThread::Prepare()
{
	Create(NULL,&Run,(void*)this);
	state=PREPARING;
}

void RCWorkerThread::Start()
{
	_mux->lock();
	if(state!=RUNNING)
	{
		_cond->signal();
		state=RUNNING;
	}
	_mux->unlock();
}

void RCWorkerThread::Suspend()
{
	_mux->lock();
	if(state==RUNNING)
		state=SUSPEND;
	_mux->unlock();
}

void *RCWorkerThread::Run(void *argv)
{
	RCWorkerThread *this_p=(RCWorkerThread*)argv;
	while(1)
	{
		this_p->_mux->lock();
		while(this_p->state!=RUNNING)
		{
			this_p->_cond->wait(*(this_p->_mux));
		}
		this_p->_mux->unlock();

		if(this_p->job)
			(*(this_p->job->handler))(this_p->job->Jobdata);

		this_p->job=NULL;
		this_p->Suspend();
	}
}

int RCWorkerThread::getState()
{
	_mux->lock();
	int temp_state=state;
	_mux->unlock();
	return temp_state;
}

RCWorkerThread::~RCWorkerThread()
{
	if(state==RUNNING)
		Join();
}








RCJob::RCJob():JobName("default"),JobID(0),Jobdata(NULL){}

RCJob::RCJob(void *jobdata)
{
	this->Jobdata=jobdata;
}

RCJob::~RCJob(){}






RCThreadManager::RCThreadManager()
{
	pool.CreateThreadPool();
	pool.InitGuard();
}

RCThreadManager::RCThreadManager(int maxsize):pool(maxsize)
{
	pool.CreateThreadPool();
	pool.InitGuard();
}

void RCThreadManager::Run(RCJob& this_job)
{
	pool.AddJob(this_job);
}

