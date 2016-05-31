/* Ben Haase
 * CS 4760
 * Assignment 6
 *$Author: o-haase $
 *$Date: 2016/05/03 03:44:37 $
 *$Log: oss.c,v $
 *Revision 1.4  2016/05/03 03:44:37  o-haase
 *Minor changes and cleanup
 *
 *Revision 1.3  2016/05/02 01:46:38  o-haase
 *Modified time progression, print physical memory to log, stat calculation, comments
 *
 *Revision 1.2  2016/05/01 20:33:28  o-haase
 *Properly fills requests and manages page tables
 *
 *Revision 1.1  2016/04/30 03:28:22  o-haase
 *Initial revision
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>
#include "page.h"

#define maxp 12
	
void os(int);
void sigint_handler(int);
void timeout_handler(int);

mem * m; //pointer to set up and access shared memory struct
int shmid; //memory var for shmat return val
pid_t up[maxp]; //hold slave process actual pid numbers

int main(int argc, char* argv[]){
	int t;
	int tkill = 60;
	//getopt for time limit if not specied time limit = 60 seconds
	if((t = getopt(argc, argv, "t::")) != -1){
		if(t == 't') tkill = atoi(argv[2]);
		else{
			perror("Error: Options character invalid");
			exit(0);
		}
	}

	srand(time(NULL)); //initialize srand

	//request and format shared memory
	shmid = shmget(SHMKEY, BUFF_SZ, 0777 | IPC_CREAT);
	if(shmid == -1){
		perror("oss: Error in shmget. \n");
		exit(1);
	}
	char * paddr = (char*)(shmat(shmid, 0, 0));
	m = (mem*)(paddr);
	mem_init(); //set up shared memory

	os(tkill); //spawn processes

	mem_cleanup(); //cleanup shared memory
	shmctl(shmid, IPC_RMID, (struct shmid_ds*)NULL); //clean up memory
	
	return 0;
}

//function to spawn the process and run the program
void os(int timekill){
	int status; //hold status of finished slave process
	int n = maxp; //number of processes
	int nprun = 0; //number of processes running
	int i, j;
	double avgt = 0; //average time a process is in the system
	int npran = 0; //numer of processes that have ran to completion
	double tc = 1.0; //time constant for logical clock progression (milliseconds)
	double ct = 0; //current time of logical clock
	double waited = 0.0; //time waited by processes
	int procsran[maxp]; //number of processes ran in each pid slot
	int fail = 0; //incremented if failure, flag for termination
	char t[3]; //hold slave process number
	FILE * otest; //log file
	double tt, wt, cu, printtimer, cub, cua; //used in stats calculation
	int freeframes = 256; //number of free frames
	physicalMem pm[256]; //array to hold the physical memory structure
	int ft, vr, avgfaults, fault, avgreqs; //used in stats calculation
	int pagereqs = 0; //number of page requests filled
	short rf;
	cua = printtimer = 0.0;
	
	//set up signal handlers and real time timer
	signal(SIGINT, sigint_handler);
	signal(SIGALRM, timeout_handler);
	alarm(timekill);


	//initialize processes ran in pid slots
	for(i = 0; i < maxp; i++) procsran[i] = 0;

	pm_init(pm);

	//main loop for oss, progress logical clock, spawn processes, run function to allocate resources	
	while((clockr() < 300) || (nprun > 0)){

		//if process has ran to completion, remove and extract stats
		for(i = 1; i < maxp; i++){
			if(m->done[i] == 1){
				for(j = 0; j < 256; j++){
					if(pm[j].proc == i){
						pm[j].page = 0;
						pm[j].proc = 0;
						pm[j].db = 0;
						pm[j].vb = 0;
						pm[j].tlp = -1.0;
					}
				}
				ct = clockr();
				avgt = (avgt + (ct - (m->c[i].timeEntered)));
				waited = (waited + m->c[i].timeWait);
				procsran[i] = procsran[i] + 1;
				m->c[i].running = 0;
				m->done[i] = 0;
				npran++;
				nprun--;
				waitpid(-1, NULL, WNOHANG);
			}	
		}

		//find open slot for new process, if one exists
		i = 1;
		while((i < maxp + 1) && (m->c[i].running == 1)){ i++;}

		//spawn new process
		if((i > 0) && (i < maxp) && (nprun < maxp) && (clockr() < 200.0)){
			clockw((((rand() % 500) + tc) / 1000.0));
			ct = clockr();
			p_init(i);
			if((up[i] = fork()) < 0){
				perror("Fork failed \n");
				fail = 1;
				return;
			}
			if(up[i] == 0){
				sprintf(t, "%i", i);
				if((execl("./uproc", "uproc", t, NULL)) == -1){
					perror("Exec failed");
					fail = 1;
					return;
				}
			}
			else{
				sem_wait(&(m->iniw));
			}
			nprun++;
		}
		
		//fill page requests
		for(i = 1; i < maxp; i++){
			sem_wait(&(m->c[i].pts));
			rf = m->reqflg[i];
			sem_post(&(m->c[i].pts));
			if(rf > -1){
				vr = fillvalreq(i, pm);
				if(vr < 2){
					if(vr < 1){
						pagereqs++;
						freeframes--;
					}
					else if(fillreq(i, pm) < 1){
						pagereqs++;
						freeframes--;
					}
				}
			}
		}

		//call the daemon
		if(freeframes < 26){
			ft = freeframes;
			freeframes = freeframes + pagedaemon(pm);
		}
		
		//progress logical clock
		cub = clockr();
		clockw((((rand() % 10) + tc) / 1000.0));
		cua += (clockr() - cub);

		//print memory map to log file
		if((printtimer == 0) || ((clockr() - printtimer) > 20.0)){
			printtimer = clockr();
			sem_wait(&(m->logsem));
			otest = fopen("osstest", "a");
			fprintf(otest, "Allocation of frames(process,page) at %f logical seconds:\n", clockr());
			j = 0;
			fprintf(otest, "     |");
			for(i = 0; i < 16; i++){
				fprintf(otest, " %02i    |", i);
			}
			fprintf(otest, "\n");
			fprintf(otest, "______");
			for(i = 0; i < 16; i++)fprintf(otest, "________");
			for(i = 0; i < 256; i++){
				if((i % 16) == 0){
					fprintf(otest, "\n");
					fprintf(otest, "%i", j);
					if(j == 0) fprintf(otest, "    | ");
					if(j > 10 && j < 100) fprintf(otest, "   | ");
					if(j > 100) fprintf(otest, "  | ");
					j = j + 16;
				}
				if(pm[i].proc == 0){
					fprintf(otest, "   .  | ");
				}
				else{
					fprintf(otest, "%02i,%02i | ", pm[i].proc, pm[i].page);
				}
			}
			fprintf(otest, "\n\n");
			fclose(otest);
			sem_post(&(m->logsem));
		}
	}
	
	//cleanup processes
	fprintf(stderr, "clock: %f \n", clockr());
	fprintf(stderr, "Logical clock timeout \n");
	for(i = 1; i < maxp; i++) if(m->c[i].running == 1) kill(up[i], SIGQUIT);
	for(i = 1; i < maxp; i++) wait();

	tt = (avgt / npran);
	wt = (waited / npran);
	cu = (cua / clockr());
	fault = m->faults;
	avgfaults = (fault / npran);
	avgreqs = (pagereqs / npran);

	//print statistics to screen
	fprintf(stderr, "Throughtput: %i processes \n", npran);
	fprintf(stderr, "Average turnaroud time: %f logical seconds/process \n", tt);
	fprintf(stderr, "Average wait time: %f logical seconds/process\n", wt);
	fprintf(stderr, "Average page faults: %i pages/process\n", avgfaults);
	fprintf(stderr, "Average page requests: %i pages/process\n", avgreqs);
	fprintf(stderr, "CPU utilization: %f \n", cu);

	//print statistics to file
	otest = fopen("osstest", "a");
	fprintf(otest, "Throughtput: %i processes \n", npran);
	fprintf(otest, "Average turnaroud time: %f logical seconds/process \n", tt);
	fprintf(otest, "Average wait time: %f logical seconds/process \n", wt);
	fprintf(otest, "Average page faults: %i pages/process\n", avgfaults);
	fprintf(otest, "Average page requests: %i pages/process\n", avgreqs);
	fprintf(otest, "CPU utilization: %f \n", cu);
	fclose(otest);
}

//signal handler for SIGINT
void sigint_handler(int s){
	int i;
	printf("\n");
	for(i = 1; i < 19; i++) if(m->c[i].running == 1) kill(up[i], SIGTERM);
	for(i = 1; i < 19; i++) wait();
	mem_cleanup();
	shmctl(shmid, IPC_RMID, (struct shmid_ds*)NULL);
	fprintf(stderr, "oss: dying due to interrupt \n");
	exit(1);
}

//signal handler for SIGALRM (timeout)
void timeout_handler(int s){
	int i;
	printf("\n");
	for(i = 1; i < 19; i++) if(m->c[i].running == 1) kill(up[i], SIGQUIT);
	for(i = 1; i < 19; i++) wait();
	mem_cleanup();
	shmctl(shmid, IPC_RMID, (struct shmid_ds*)NULL);
	fprintf(stderr, "oss: dying due to timeout \n");
	exit(1);
}
