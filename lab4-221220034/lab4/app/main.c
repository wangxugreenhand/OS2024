#include "lib.h"
#include "types.h"
#define N1 6
#define N2 5
void producer_consume()
{
	sem_t empty, full, mutex;
	sem_init(&empty, N1); //缓冲区为N
	sem_init(&full, 0);
	sem_init(&mutex, 1);
	int i = 0;
	for(;i < 4; i++) //生产者
	{
		int ret = fork();
		if (ret == -1)
		{
			printf("Failed to fork.\n");
		}
		else if (ret == 0) 
		{
			while(1){
				int id  = getpid();
				sem_wait(&empty);
				// sleep(128);
				sem_wait(&mutex);
				printf("Producer %d: Producing.\n", id);
				sleep(128);
				sem_post(&mutex);
				// sleep(128);
				sem_post(&full);
				sleep(128);
			}
		}
		// printf("i: %d\n", i);
	}
	while(1)  //消费者
	{
		sem_wait(&full);
		sem_wait(&mutex);
		printf("Consumer : Consuming.\n");
		sleep(128);
		sem_post(&mutex);
		sem_post(&empty);
		sleep(128);
	}
}

void Philosophers()
{
	sem_t chopstick[5];
	for(int i = 0; i < N2; i++)
	{
		sem_init(&chopstick[i], 1);
	}
	int i = 0;
	for(;i < 5; i++) //哲学家
	{
		int ret = fork();
		if (ret == -1)
		{
			printf("Failed to fork.\n");
		}
		else if (ret == 0) 
		{
			while(1){
				int id  = getpid();
				printf("Philosopher %d: think\n", id);
				if(i % 2 == 0)
				{
					sem_wait(&chopstick[i]);
					sem_wait(&chopstick[(i+1)%5]);
				}
				else
				{
					sem_wait(&chopstick[(i+1)%5]);
					sem_wait(&chopstick[i]);
				}
				printf("Philosopher %d: eat\n", id);
				sem_post(&chopstick[i]);
				sem_post(&chopstick[(i+1)%5]);
				sleep(128);
			}
		}
	}
	while (1)
	{
		sleep(128);
	}
	
}

void Read_Write()
{
	sem_t WriteMutex, CountMutex;
	int Rcount = 0;
	sem_init(&WriteMutex, 1);
	sem_init(&CountMutex, 1);
	//sem_init(&Rcount, 0);
	int i = 0;
	for(i = 0; i < 3; i++) //写者
	{
		int ret = fork();
		if (ret == -1)
		{
			printf("Failed to fork.\n");
		}
		else if (ret == 0) 
		{
			while(1){
				int id  = getpid();
				sem_wait(&WriteMutex);
				printf("Writer %d: write\n", id);
				//sleep(128);
				sem_post(&WriteMutex);
				sleep(128);
			}
		}
	}
	for(i = 0;i < 3; i++) //读者
	{
		int ret = fork();
		if (ret == -1)
		{
			printf("Failed to fork.\n");
		}
		else if (ret == 0) 
		{
			while(1){
				int id  = getpid();
				sem_wait(&CountMutex);
				if(Rcount == 0)
					sem_wait(&WriteMutex);
				Rcount++;
				sem_post(&CountMutex);
				printf("Reader %d: read, total %d reader\n", id, Rcount);
				//sleep(128);
				sem_wait(&CountMutex);
				Rcount--;
				if(Rcount == 0)
					sem_post(&WriteMutex);
				sem_post(&CountMutex);
				sleep(128);
			}
		}
	}
	while(1){
		sleep(128);
	}
}


int uEntry(void)
{
	// For lab4.1
	// Test 'scanf'
	int dec = 0;
	int hex = 0;
	char str[6];
	char cha = 0;
	int ret = 0;
	while (1)
	{
		printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
		ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
		printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
		if (ret == 4)
			break;
	}

	// For lab4.2
	// Test 'Semaphore'
	int i = 4;

	sem_t sem;
	printf("Father Process: Semaphore Initializing.\n");
	ret = sem_init(&sem, 2);
	if (ret == -1)
	{
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}

	ret = fork();
	if (ret == 0)
	{
		while (i != 0)
		{
			i--;
			printf("Child Process: Semaphore Waiting.\n");
			sem_wait(&sem);
			printf("Child Process: In Critical Area.\n");
		}
		printf("Child Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}
	else if (ret != -1)
	{
		while (i != 0)
		{
			i--;
			printf("Father Process: Sleeping.\n");
			sleep(128);
			printf("Father Process: Semaphore Posting.\n");
			sem_post(&sem);
		}
		printf("Father Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
	}

	// For lab4.3
	// TODO: You need to design and test the problem.
	// Note that you can create your own functions.
	// Requirements are demonstrated in the guide.
	//producer_consume();
	//Philosophers();
	Read_Write();

	return 0;
}
