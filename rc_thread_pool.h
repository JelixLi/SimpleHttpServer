#ifndef RC_THREAD_POOL_H
#define RC_THREAD_POOL_H

#include <pthread.h>
#include <string>
#include <string.h>
#include <deque>
#include <map>

// encapsulation  of  the  pthread mutex 
class RCThreadMutex;
// base class of all sepcific jobs ( which you want to do in the thread)  
class RCJob;		
//base class of  all  threads
class RCThread;		
//subclass which exteends from the RCThread and it represents for all specific work thread 
class RCWorkerThread;
//encapsulation of pthread cond
class RCThreadCond;
//implementation  of  the  thread pool
class RCThreadPool;
//manager class which hide the inside implementation
class RCThreadManager;


class RCJob
{
public:
	RCJob();
	RCJob(void *jobdata);
	virtual ~RCJob();

	friend RCWorkerThread;

	void setJobID(int id){ JobID=id; }
	int getJobID() const { return JobID; }
	void setJobName(std::string& name){ JobName=name; }
	std::string getJobName() const { return JobName; }

	void *(*handler)(void*);  // the working function
protected:
	std::string JobName;
	int JobID;
	void *Jobdata;
};

class RCThreadMutex
{
public:
	RCThreadMutex();
    ~RCThreadMutex();

    friend class RCThreadCond;

    void lock();
    void unlock();

private:
    pthread_mutex_t mutex;
};

class RCThreadCond
{
public:
	RCThreadCond();
	~RCThreadCond();

	void signal();
	void wait(RCThreadMutex& mux);
private:
	pthread_cond_t cond;  
};

class RCThread
{
public:
	RCThread(); 
	virtual ~RCThread();

	void Join();
	void Create(pthread_attr_t *attr,void *(*handler)(void*),void *arg);
	inline pthread_t getThreadID() const{return ThreadID;}
	inline void setThreadID(int id){this->ThreadID=id;}
	//inline int getRetCode() const{return retCode;}
	inline std::string getThreadName() const{return ThreadName;}
	inline void setThreadName(std::string &str){this->ThreadName=str;}
protected:
	std::string ThreadName;
	//int retCode;
	pthread_t ThreadID;
};


#define NOTSTART 0
#define RUNNING 1
#define SUSPEND 2
#define PREPARING 3

class RCWorkerThread:public RCThread
{
public:
	RCWorkerThread();
	RCWorkerThread(RCThreadCond& cond,RCThreadMutex& mux);
	~RCWorkerThread();

	void setJob(RCJob& this_job);

	void Prepare();
	void Start();
	void Suspend();
	int getState();
	//void *(*handler)(void*);
	static void *Run(void *argv);

private:
	RCJob *job;
	RCThreadMutex* _mux;
	RCThreadCond* _cond;
	int state;
};

class RCThreadPool
{
public:
	RCThreadPool();
	RCThreadPool(int maxsize);
	~RCThreadPool(){};

	void CreateThreadPool();
	void AppendToIdleList(RCWorkerThread* t);
	void MoveToBusyList(RCWorkerThread* t);

	int getIdleSize();
	int getBusySize();

	void AddJob(RCJob& this_job);

	void InitGuard();
	static void *GuardThread(void *argv);
private:
	RCThreadMutex IdleMutex;
	RCThreadMutex BusyMutex;
	std::deque<RCWorkerThread*> IdleList;
	std::deque<RCWorkerThread*> BusyList;
	int m_InitNum;
	int idlesize;
	int busysize;
};



class RCThreadManager
{
public:
	RCThreadManager();
	RCThreadManager(int maxsize);
	~RCThreadManager(){}

	void Run(RCJob& this_job);  // the working function
private:
	RCThreadPool pool;
};




#endif