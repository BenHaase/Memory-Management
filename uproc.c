/* Ben Haase
 * CS 4760
 * Assignment 6
 *$Author: o-haase $
 *$Date: 2016/05/02 01:46:38 $
 *$Log: uproc.c,v $
 *Revision 1.3  2016/05/02 01:46:38  o-haase
 *Added mem access time calculation, and logging of statistics to file, comments
 *
 *Revision 1.2  2016/05/01 20:33:28  o-haase
 *Properly makes requests and terminates after 1000 +- 100 requests have been made
 *
 *Revision 1.1  2016/04/30 03:28:22  o-haase
 *Initial revision
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
#include "page.h"

void pmsg(char*, char*);
void sigtimeout_handler(int);
void sigctlc_handler(int);

//globals for use in signal handlers
char * paddr; //memory address returned from shmat
int pn; //integer representation of process number (from master)
char * pnc; //character representation of process number (from master)
mem * m;  //format and reference shared memory struct

int main(int argc, char* argv[]){
	//setup signal handlers (ignore SIGINT that will be intercepted by master)
	//signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, sigtimeout_handler);
	signal(SIGTERM, sigctlc_handler);
	signal(SIGINT, SIG_IGN);

	//set process number and char representation
	pn = atoi(argv[1]);
	pnc = argv[1];
	srand((unsigned)(time(NULL) + pn)); //initialize srand

	//mount shared memory
	int shmid = shmget(SHMKEY, BUFF_SZ, 0777);
	if(shmid == -1){
		perror("User process: Error in shmget opening.");
		exit(1);
	}
	paddr = (char*)(shmat(shmid, 0, 0));
	m = (mem*)(paddr);

	//set time process entered the system and its status
	m->c[pn].timeEntered = clockr();
	m->c[pn].timeWait = 0.0;
	m->c[pn].running = 1;

	int reqend, reqmade, r, rw;
	char io;
	double avgaccess = 0.0;
	double cl = 0.0;
	FILE * otest;

	reqend = (rand() % 200) + 900; //set number of requests to make before termination

	sem_post(&(m->iniw));

	//make requests until termination
	while(reqmade < reqend){
		rw = rand() % 2;
		io = 0;
		if(rw == 1){ io = 1;}
		r = rand() % 32;
		cl = clockr();
		pagereq(pn, r, io);
		m->c[pn].timeWait += clockr() - cl;
		
		reqmade++;
	}
	
	//print stats to screen and file
	avgaccess = (m->c[pn].timeWait) / reqmade;
	fprintf(stderr, "clock: %f \n", clockr());
	fprintf(stderr, "%i is done.  Requests filled: %i. \n", pn, reqmade);
	fprintf(stderr, "%i's average memory access time: %f logical seconds \n", pn, avgaccess);
	sem_wait(&(m->logsem));
	otest = fopen("osstest", "a");
	fprintf(otest, "clock: %f \n", clockr());
	fprintf(otest, "%i is done.  Requests filled: %i. \n", pn, reqmade);
	fprintf(otest, "%i's average memory access time: %f logical seconds \n\n", pn, avgaccess);
	fclose(otest);
	sem_post(&(m->logsem));
	

	//set status to done for resource and statistic extraction
	m->done[pn] = 1;

	//dismount memory and print finished message
	shmdt(paddr);
	return 0;
}

//function to print process messages with time
void pmsg(char *p, char *msg){
	time_t tm;
	char * tms;
	time(&tm);
	tms = ctime(&tm);
	fprintf(stderr, "Process: %02s %s at %.8s.\n", p, msg, (tms + 11));
}

//signal handler for SIGQUIT (sent from master on timeout)
void sigtimeout_handler(int s){
	shmdt(paddr);
	fprintf(stderr, "User process: %02s dying due to timeout \n", pnc);
	exit(1);
}

//signal handle for SIGTERM (sent from master on ^C)
void sigctlc_handler(int s){
	shmdt(paddr);
	fprintf(stderr, "User process: %02s dying due to interrupt \n", pnc);
	exit(1);
}
