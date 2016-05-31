/* Ben Haase
 * CS 4760
 * Assignment 6
 * $Author: o-haase $
 * $Date: 2016/05/02 01:46:38 $
 * $Log: page.h,v $
 * Revision 1.3  2016/05/02 01:46:38  o-haase
 * minor changes
 *
 * Revision 1.2  2016/05/01 20:33:28  o-haase
 * Added structures to manage page tables and processes
 *
 * Revision 1.1  2016/04/30 03:28:22  o-haase
 * Initial revision
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>

//key and size for shared memory
#define SHMKEY 56
#define maxp 12
#define BUFF_SZ sizeof(mem)

//process page table struct
typedef struct{
	unsigned short p; //page
	char db; //dirty bit
	char vb; //valid bit
	} processPage;

//child process struct process page table, semaphores and info
typedef struct{
	double timeEntered;
	double timeWait;
	sem_t rs; //request semaphore
	sem_t pts; //page table semaphore
	char running;
	processPage pt[32]; //process page table
}child;

//os process, valid page, and logical clock struct
typedef struct{
	char done[maxp]; //if process ran to completion
	child c[maxp];
	double lclock;
	sem_t cls;
	sem_t iniw;
	sem_t logsem;
	short reqflg[maxp]; //processes request
	double reqtime[maxp];
	char io[maxp];
	int faults;
}mem;

//oss physical memory structure
typedef struct{
	short page; //page number
	short proc; //process number
	char db; //dirty bit
	char vb; //valid bit
	double tlp;  //time last page
}physicalMem;

//struct to hold physical memory table use order
typedef struct{
	double tlp;
	int page;
}physmemsort;

//function prototypes
	void mem_init();
	double clockr();
	void clockw(double);
	void mem_cleanup();
	void p_init(int);
	void pm_init(physicalMem *);
	void sortpm(physicalMem *);
	void rmpm(physicalMem *, int);
	void pagereq(int, int, char);
	int fillreq(int, physicalMem *);
	int fillvalreq(int, physicalMem *);
	int pagedaemon(physicalMem * pm);
