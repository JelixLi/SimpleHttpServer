#include "shttpd.h"
#include <iostream>
#include <ctype.h>
#include <algorithm>
#include <unistd.h>
#include <sstream>

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

struct conf_opts Configure::opt={
"/home/jieli/code/SHTTPD/www/cgi_bin",  //CGIRoot
"index.html", //DefaultFile
"/home/jieli/code/SHTTPD/www", //DocumentRoot
"/home/jieli/code/SHTTPD/shttpd.conf", //ConfigFile
8080, //ListenPort
20,  //MaxClient
0,  //Timeout
};

// struct conf_opts Configure::opt={
// "/home/jieli/code/SHTTPD/www/cgi_bin",  //CGIRoot
// "index.html", //DefaultFile
// "/home/admin/SimpleHttpServer/www", //DocumentRoot
// "/home/admin/SimpleHttpServer/shttpd.conf", //ConfigFile
// 80, //ListenPort
// 20,  //MaxClient
// 0,  //Timeout
// };

Configure::Configure()
{
	opt.InitClient=5;
}

void Configure::display_usage(){}

void Configure::print()
{
	std::cout<<"CGIRoot:	"<<opt.CGIRoot<<std::endl
			<<"DefaultFile:	"<<opt.DefaultFile<<std::endl
			<<"DocumentRoot:	"<<opt.DocumentRoot<<std::endl
			<<"ConfigFile:	"<<opt.ConfigFile<<std::endl
			<<"ListenPort:	"<<opt.ListenPort<<std::endl
			<<"MaxClient:	"<<opt.MaxClient<<std::endl
			<<"Timeout:	"<<opt.Timeout<<std::endl
			<<"InitClient:	"<<opt.InitClient<<std::endl;
}

int Configure::Para_CmdParse(int argc,char **argv)
{
	char ret;
	char *value;
	while((ret=getopt_long(argc,argv,shortopts,longopts,NULL))!=-1)
	{
		switch(ret)
		{
			case 'c':
			{
				value=optarg;
				if(value)
				{
					this->opt.CGIRoot=value;
				}
				break;
			}			
			case 'd':
			{
				value=optarg;
				if(value)
				{
					this->opt.DocumentRoot=value;
				}
				break;
			}
			case 'f':
			{
				value=optarg;
				if(value)
				{
					this->opt.ConfigFile=value;
				}
				break;
			}
			case 'l':
			{
				value=optarg;
				if(value)
				{
					int port=strtol(value,NULL,10);
					this->opt.ListenPort=port;
				}
				break;
			}
			case 'm':
			{
				value=optarg;
				if(value)
				{
					int maxnum=strtol(value,NULL,10);
					this->opt.MaxClient=maxnum;
				}
				break;
			}
			case 't':
			{
				value=optarg;
				if(value)
				{
					int time=strtol(value,NULL,10);
					this->opt.Timeout=time;
				}
				break;
			}
			case 'h':
			{
				display_usage();
				break;
			}
			default:
				std::cout<<"Command Error"<<std::endl;
				break;
		}
	}
}

int Configure::Para_FileParse()
{
	Para_FileParse(this->opt.ConfigFile);
}


int Configure::Para_FileParse(const std::string &file)
{
	FILE *fp;
	if((fp=fopen(file.c_str(),"r"))==NULL)
	{
		std::cout<<"ERROR:  Configure File Open Failed"<<std::endl;
		exit(1);
	}

	char val;
	int flag=0;
	int type_flag=0;
#define CGIROOT 1
#define DOCUMENTROOT 2
#define DEFAULTFILE 3
#define CONFIGFILE 4
#define LISTENPORT 5
#define MAXCLIENT 6
#define INITCLIENT 7
#define TIMEOUT 8
	std::string list;
	while((val=fgetc(fp))!=EOF)
	{
		if(val!='#')
		{
			if(isspace(val)||val=='\n'||val=='=')
				flag=0;
			// else if(isalpha(val)||val=='/'||val=='.'||val=='_')
			// {
			// 	list+=val;
			// 	flag=1;
			// }
			else 
			{
				// std::cout<<"ERROR: Invalid Character: "<<val<<" in the Conf File"<<std::endl;
				// exit(1);

				list+=val;
				flag=1;

			}

			if(!flag)
			{
				if(list.empty())
					continue;

				if(type_flag)
				{
					switch(type_flag)
					{
						case CGIROOT:	this->opt.CGIRoot=list; break;
						case DEFAULTFILE:	this->opt.DefaultFile=list; break;
						case DOCUMENTROOT:	this->opt.DocumentRoot=list; break;
						case CONFIGFILE:  this->opt.ConfigFile=list; break;
						case MAXCLIENT:
						{
							int maxnum=strtol(list.c_str(),NULL,10);
							this->opt.MaxClient=maxnum;
							break;
						}
						case INITCLIENT:
						{
							int num=strtol(list.c_str(),NULL,10);
							this->opt.InitClient=num;
							break;			
						}
						case LISTENPORT:
						{
							int port=strtol(list.c_str(),NULL,10);
							this->opt.ListenPort=port;
							break;
						}
						case TIMEOUT:
						{
							int time=strtol(list.c_str(),NULL,10);
							this->opt.Timeout=time;
							break;	
						}
					}
					type_flag=0;
					list="";
					continue;
				}

				if(list=="CGIRoot")
					type_flag=CGIROOT;
				else if(list=="DefaultFile")
					type_flag=DEFAULTFILE;
				else if(list=="DocumentRoot")
					type_flag=DOCUMENTROOT;
				else if(list=="ConfigFile")
					type_flag=CONFIGFILE;
				else if(list=="ListenPort")
					type_flag=LISTENPORT;
				else if(list=="MaxClient")
					type_flag=MAXCLIENT;
				else if(list=="InitClient")
					type_flag=INITCLIENT;
				else if(list=="Timeout")
					type_flag=TIMEOUT;

				list="";
			}

		}
	}


	if(fclose(fp)!=0)
	{
		std::cout<<"ERROR:  Configure File Close Failed"<<std::endl;
		exit(1);
	}
}

Configure::~Configure(){}


RCFile::RCFile(){}

RCFile::~RCFile()
{
	if(fclose(fp)!=0)
	{
		perror("close file");
		exit(1);
	}
	FileMutex.unlock();
}

void RCFile::openfile(std::string &src)
{
	FileMutex.lock();
	if((fp=fopen(src.c_str(),"r"))==NULL)
	{
		perror("openfile");
		exit(1);
	}
}


void RCFile::read(char *buffer,int size)
{
	fread(buffer,size,1,fp);
}




void HttpManager::Request_Parse(std::string src)
{
	std::string list;
	int count=0;
	for(int i=0;i<src.size();i++)
	{
		if(count<3)
		{
			if(src[i]==' '||src[i]=='\r')
			{
				count++;
				switch(count)
				{
					case 1:
					{
						transform(list.begin(),list.end(),list.begin(),toupper);
						if(list=="GET")
						{
							method="GET";
						}
						break;
					}
					case 2:
					{
						URL=list;
						break;
					}
					case 3:
					{
						version=list;
						break;
					}
				}
				list="";
			}	
		}
		else if(src[i]=='\n')
			continue;
		else if(src[i]=='\r')
		{
			int pos=list.find(":");
			request_data.push_back(std::pair<std::string,std::string>(list.substr(0,pos+1),list.substr(pos+1,list.size()-pos)));
			list="";
			continue;
		}

		list+=src[i];
	}
}

std::string IntToString(int num)
{
	std::stringstream ss;
	std::string str;
	ss<<num;
	ss>>str;
	return str;
}

void HttpManager::Send_Response(RCSocket &rc)
{
	std::string response;
	response.append(version+" "+IntToString(status)+" "+"Thanks For Using JieLi's Server\r\n");
	response.append("Content-Type: text/html;charset=utf-8\r\n");
	response.append("\r\n");
	memcpy(rc.getSendBuffer(),response.c_str(),response.size()+1);
	rc.rc_send();
	RCFile rc_f;
	std::string source;
	source.append(opt.DocumentRoot);
	source+='/';
	source.append(opt.DefaultFile);
	rc_f.openfile(source);
	while(!feof(rc_f.getFPointer()))
	{
		rc_f.read(rc.getSendBuffer(),rc.getBufferSize());
		rc.rc_send();
	}
}

void HttpManager::Handle_Request(RCSocket &rc)
{
	if(method=="GET")
	{
		Send_Response(rc);
	}
}



void HttpManager::print()
{
	#define myprint(x) std::cout<<x<<std::endl;

	myprint("method:  "+method);
	myprint("URL:  "+URL);
	myprint("version:  "+version);
	for(int i=0;i<request_data.size();i++)
	{
		myprint(request_data[i].first+" "+request_data[i].second);
	}
}


RCSocket::RCSocket():port(opt.ListenPort),clientnum(opt.MaxClient),client_addr_len(sizeof(client_addr)),buffersize(1024)
{
	memset(&server_addr,0,sizeof(server_addr));
	memset(&client_addr,0,sizeof(client_addr));
	send_buffer=new char[buffersize];
	recv_buffer=new char[buffersize];
}

RCSocket::RCSocket(RCSocket& rc)
{
	memcpy(&this->server_addr,&rc.server_addr,sizeof(server_addr));
	memcpy(&this->client_addr,&rc.client_addr,sizeof(client_addr));
	this->client_addr_len=rc.client_addr_len;
	this->server_sock=rc.server_sock;
	this->client_sock=rc.client_sock;
	this->clientnum=rc.clientnum;
	this->port=rc.port;
	this->buffersize=rc.buffersize;
	this->send_buffer=new char[buffersize];
	memcpy(this->send_buffer,rc.send_buffer,buffersize);
	this->recv_buffer=new char[buffersize];
	memcpy(this->recv_buffer,rc.recv_buffer,buffersize);
}

void RCSocket::rc_socket()
{
	server_sock=socket(AF_INET,SOCK_STREAM,0);
	if(server_sock<0)
	{
		perror("socket");
		exit(1);
	}
}

void RCSocket::rc_bind()
{
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(port);
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	std::cout<<port<<std::endl;
	if(bind(server_sock,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
	{
		perror("bind");
		exit(1);
	}
}

void RCSocket::rc_listen()
{
	if(listen(server_sock,clientnum)<0)
	{
		perror("listen");
		exit(1);
	}
}

void RCSocket::rc_accept()
{
	client_sock=accept(server_sock,(struct sockaddr*)&client_addr,&client_addr_len);
	if(client_sock<0)
	{
		perror("accept");
		exit(1);
	}
}

void RCSocket::rc_send()
{
	send(client_sock,send_buffer,buffersize,0);
	memset(send_buffer,0,buffersize);
}


void RCSocket::rc_recv()
{
	memset(recv_buffer,0,buffersize);
	recv(client_sock,recv_buffer,buffersize,0);
}

void RCSocket::CloseClient()
{
	close(client_sock);
}

void RCSocket::CloseServer()
{
	close(server_sock);
}

RCSocket::~RCSocket()
{
	delete [] recv_buffer;
	delete [] send_buffer;
}

void *JobDoWork::Run(void *argv)
{
	RCSocket *rc=(RCSocket*)argv;
	HttpManager rc_http;
	while(1)
	{
		rc->rc_accept();
		rc_http.setStatus(200);

		rc->rc_recv();
		rc_http.Request_Parse(rc->getRecvBuffer());
		rc_http.Handle_Request(*rc);

		rc->CloseClient();
	}
}


void Worker::Run()
{
	RCSocket rc;
	rc.rc_socket();
	rc.rc_bind();
	rc.rc_listen();
	for(int i=0;i<clientnum;i++)
	{
		RCSocket rc_temp(rc);
		JobDoWork rc_work((void*)&rc_temp);
		manager.Run(rc_work);
	}
}