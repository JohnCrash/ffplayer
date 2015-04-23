#include "SDLImp.h"

/*
SDL Thread ��������ʹ��std::thread��װ
*/
//�ȼ���SDL_CreateMutex
mutex_t* createMutex()
{
	return new mutex_t();
}

//�ȼ���SDL_LockMutex
int lockMutex(mutex_t *mutex)
{
	mutex->lock();
	return 0;
}

//�ȼ���SDL_UnlockMutex
int unlockMutex(mutex_t *mutex)
{
	mutex->unlock();
	return 0;
}

//�ȼ���SDL_DestroyMutex
void destroyMutex(mutex_t *mutex)
{
	delete mutex;
}

//�ȼ���SDL_CreateCond
cond_t * createCond()
{
	return new cond_t();
}

//�ȼ���SDL_CondWait
//��Ҫʹ�ñ�����
int waitCond(cond_t* pcond, mutex_t *pmutx)
{
	std::unique_lock<mutex_t> lk(*pmutx);
	pcond->wait(lk);
	return 0;
}

//�ȼ���SDL_CondSignal
int signalCond(cond_t* pcond)
{
	pcond->notify_one();
	return 0;
}

//�ȼ���SDL_DestroyCond
void destroyCond(cond_t *pcond)
{
	delete pcond;
}

//�ȼ���SDL_CondWaitTimeout
//��Ҫʹ�ñ�����
int waitCondTimeout(cond_t* pcond, mutex_t* pmutex, unsigned int ms)
{
	std::unique_lock<mutex_t> lk(*pmutex);
	if (pcond->wait_for(lk, std::chrono::milliseconds(ms)) == std::cv_status::timeout)
		return 1;// SDL_MUTEX_TIMEDOUT;
	else
		return 0;
}

//�ȼ���SDL_CreateThread
thread_t* createThread(int(*func)(void*), void *p)
{
	return new thread_t(func,p);
}

//�ȼ���SDL_WaitThread,�ȴ��߳̽���
void waitThread(thread_t *pthread, int* status)
{
	pthread->join();
	delete pthread;
}

/*
	��ʱms����
*/
void Delay(Uint32 ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}