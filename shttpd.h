#ifndef SHTTPD
#define SHTTPD

#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <utility>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
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

class RCSocket;

class HttpManager;

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


struct conf_opts
{
	std::string CGIRoot;
	std::string DefaultFile;
	std::string DocumentRoot;
	std::string ConfigFile;
	int ListenPort;
	int MaxClient;
	int Timeout;
	int InitClient;
};

static const char *shortopts="c:d:f:ho:l:m:t:";

static const struct option longopts[]={
	{"CGIRoot",		required_argument, NULL,'c'},
	{"DefaultFile",	required_argument,	NULL,'d'},
	{"DocumentRoot",required_argument,	NULL,'o'},
	{"ConfigFile",	required_argument,	NULL,'f'},
	{"ListenPort",	required_argument, NULL,'l'},
	{"MaxClient",	required_argument,	NULL,'m'},
	{"Timeout",		required_argument,	NULL,'t'},
	{"Help",		no_argument,	NULL,'h'},
	{0,0,0,0}
};

class Configure
{
public:
	Configure();
	virtual ~Configure();

	void display_usage();
	int Para_CmdParse(int argc,char **argv);
	int Para_FileParse();
	int Para_FileParse(const std::string &file);
	void print();

protected:
	static conf_opts opt;
};



class RCFile
{
public:
	RCFile();
	~RCFile();

	void openfile(std::string &src);
	void read(char *buffer,int size);
	FILE *getFPointer() { return fp; }
private:
	FILE *fp;
	RCThreadMutex FileMutex;
};

enum Rc_Error
{
	ERROR301=301,ERROR302=302,ERROR303=303,ERROR304=304,ERROR305=305,ERROR307=307,
	ERROR400=400,ERROR401=401,ERROR402=402,ERROR403=403,ERROR404=404,ERROR405=405,ERROR406=406,
	ERROR407=407,ERROR408=408,ERROR409=409,ERROR410=410,ERROR411=411,ERROR412=412,ERROR413=413,
	ERROR414=414,ERROR415=415,ERROR416=416,ERROR417=417,ERROR500=500,ERROR501=501,ERROR502=502,
	ERROR503=503,ERROR504=504,ERROR505=505
};

struct Rc_Errors
{
	enum Rc_Error error_code;
	std::string content;
	std::string msg;
};

static struct Rc_Errors rc_error_http[]={
	{ERROR301,"Error: 301","A permanent move"},
	{ERROR302,"Error: 302","create"},
	{ERROR303,"Error: 303","look at other parts"},
	{ERROR304,"Error: 304","read only"},
	{ERROR305,"Error: 305","the user agent"},
	{ERROR307,"Error: 307","Temporary retransmission"},
	{ERROR400,"Error: 400","bad request"},
	{ERROR401,"Error: 401","unauthorized"},
	{ERROR402,"Error: 402","necessary payment"},
	{ERROR403,"Error: 403","disable"},
	{ERROR404,"Error: 404","not found"},
	{ERROR405,"Error: 405","not allowed"},
	{ERROR406,"Error: 406","not accept"},
	{ERROR407,"Error: 407","require proxy authentication"},
	{ERROR408,"Error: 408","Time out"},
	{ERROR409,"Error: 409","Conflict"},
	{ERROR410,"Error: 410","Stop"},
	{ERROR411,"Error: 411","Needed Length"},
	{ERROR412,"Error: 412","pretrement failed"},
	{ERROR413,"Error: 413","Excessive request entity"},
	{ERROR414,"Error: 414","request url too large"},
	{ERROR415,"Error: 415","unsupported media type"},
	{ERROR416,"Error: 416","the scope of request not satisfied"},
	{ERROR417,"Error: 417","expect failure"},
	{ERROR500,"Error: 500","server internal error"},
	{ERROR501,"Error: 501","can't achieve"}
};

class RCErrorManager
{

};


enum RC_Type
{
	RC_HTML,RC_TXT,RC_JPG
};

struct RC_Types
{
	enum RC_Type type;
	std::string extension;
	int ext_len;
	std::string location;
};

class HttpManager:public Configure
{
public:
	HttpManager(){};
	~HttpManager(){};

	void Request_Parse(std::string src);
	void Handle_Request(RCSocket &rc);
	void Send_Response(RCSocket &rc);
	void print();
	void setStatus(int status) { this->status=status; }
private:
	std::string method;
	std::string URL;
	std::string version;
	std::vector<std::pair<std::string,std::string> > request_data;
	int status;
	std::string description;
};

class RCSocket:public Configure
{
public:
	RCSocket();
	RCSocket(RCSocket& rc);
	~RCSocket();

	void rc_socket();
	void rc_bind();
	void rc_listen();
	void rc_accept();
	void rc_send();
	void rc_recv();
	void CloseClient();
	void CloseServer();

	char *getRecvBuffer(){ return recv_buffer; }
	char *getSendBuffer(){ return send_buffer; }
	int getBufferSize(){ return buffersize; }
private:
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	int server_sock;
	int client_sock;
	int clientnum;
	int port;
	int buffersize;
	char *send_buffer;
	char *recv_buffer;
};


class JobDoWork:public RCJob
{
public:
	JobDoWork(void *jobdata):RCJob(jobdata){this->handler=&Run;}
	~JobDoWork(){}

	static void *Run(void *argv);
};

class Worker:public Configure
{
public:
	Worker():manager(opt.MaxClient),clientnum(opt.MaxClient){}
	~Worker(){}

	void Run();

private:
	int clientnum;
	RCThreadManager manager;
};

#endif