/* Ben Haase
 * CS 4760
 * Assignment 6
 * $Author: o-haase $
 * $Date: 2016/05/02 01:46:38 $
 * $Log: page.c,v $
 * Revision 1.3  2016/05/02 01:46:38  o-haase
 * Added mem access time calculation, and logging of statistics to file
 *
 * Revision 1.2  2016/05/01 20:33:28  o-haase
 * Properly makes requests and terminates after 1000 +- 100 requests have been made
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
#include "page.h"

	extern mem * m;

	//function to initialize struct values and semaphores in shared memory
	void mem_init(){
		int i, j;
		for(i = 0; i < 12; i++){
			m->done[i] = 0;
			m->reqflg[i] = -1;
			m->reqtime[i] = 0.0;
			m->io[i] = 0;
			m->c[i].timeEntered = 0.0;
			m->c[i].timeWait = 0.0;
			m->c[i].running = 0;
		}
		m->lclock = 0.0;
		m->faults = 0;
		if((sem_init(&(m->cls), 1, 1)) < 0) perror("Semaphore cls initialization error");
		if((sem_init(&(m->iniw), 1, 0)) < 0) perror("Semaphore iniw initialization error");
		if((sem_init(&(m->logsem), 1, 1)) < 0) perror("Semaphore logsem initialization error");
		for(i = 0; i < 12; i++){
			if((sem_init(&(m->c[i].rs), 1, 0)) < 0) perror("Semaphore rs initialization error");
			if((sem_init(&(m->c[i].pts), 1, 1)) < 0) perror("Semaphore pts initialization error");
		}
	}

	//semaphore protected clock read function
	double clockr(){
		double t;
		sem_wait(&(m->cls));
		t = m->lclock;
		sem_post(&(m->cls));
		return t;
			
	}

	//semaphore protected clock write function
	void clockw(double a){
		sem_wait(&(m->cls));
		(m->lclock) = (m->lclock) + a;
		sem_post(&(m->cls));
	}

	//semaphore cleanup function for termination
	void mem_cleanup(){
		int i;
		if((sem_destroy(&(m->cls))) < 0) perror("Semaphore cls destruction error");
		if((sem_destroy(&(m->iniw))) < 0) perror("Semaphore iniw destruction error");
		if((sem_destroy(&(m->logsem))) < 0) perror("Semaphore logsem destruction error");
		for(i = 0; i < 12; i++){
			if((sem_destroy(&(m->c[i].rs))) < 0) perror("Semaphore rs initialization error");
			if((sem_destroy(&(m->c[i].pts))) < 0) perror("Semaphore pts destruction error");
		}	
	}

	//initialize a new process table
	void p_init(int x){
		int i;
		m->done[x] = 0;
		m->reqflg[x] = -1;
		m->reqtime[x] = 0.0;
		m->io[x] = 0;
		m->c[x].timeEntered = 0.0;
		m->c[x].timeWait = 0.0;
		m->c[x].running = 1;
		for(i = 0; i < 32; i++){
			m->c[x].pt[i].p = 0;
			m->c[x].pt[i].db = 0;
			m->c[x].pt[i].vb = 0;
		}
	}

	//initialize pm for oss
	void pm_init(physicalMem * pm){
		int i;
		for(i = 0; i < 256; i++){
			pm[i].page = 0;
			pm[i].proc = 0;
			pm[i].db = 0;
			pm[i].vb = 0;
			pm[i].tlp = -1.0;
		}
	}

	//sort the physical memory table and remove or mark pages for removal
	int pagedaemon(physicalMem * pm){
		int i, j;
		int ff = 0;
		physmemsort pms[256];
		for(i = 0; i < 256; i++){
			pms[i].page = i;
			pms[i].tlp = pm[i].tlp;
		}
		physmemsort t;
		for(i = 255; i > -1; i--){
			for(j = 1; j < (i + 1); j++){
				if(pms[j-1].tlp > pms[j].tlp){
					t.page = pms[j-1].page;
					t.tlp = pms[j-1].tlp;
					pms[j-1].page = pms[j].page;
					pms[j-1].tlp = pms[j].tlp;
					pms[j].page = t.page;
					pms[j].tlp = t.tlp;
				}
			}
		}
		
		for(i = 0; i < 25; i++){
			j = pms[i].page;
			if(pm[j].vb == 0){
				rmpm(pm, j);
				ff++;
			}
			else{
				pm[j].vb = 0;
				sem_wait(&(m->c[pm[j].proc].pts));
				m->c[pm[j].proc].pt[pm[j].page].vb = 0;
				sem_post(&(m->c[pm[j].proc].pts));
			}
		}

		return ff;
	}

	//remove a page
	void rmpm(physicalMem * pm, int x){
		sem_wait(&(m->c[pm[x].proc].pts));
		m->c[pm[x].proc].pt[pm[x].page].vb = 0;
		sem_post(&(m->c[pm[x].proc].pts));
		pm[x].page = 0;
		pm[x].proc = 0;
		pm[x].db = 0;
		pm[x].vb = 0;
		pm[x].tlp = -1.0;
	}

	//process page request
	void pagereq(int p, int r, char rw){
		sem_wait(&(m->c[p].pts));
		m->reqflg[p] = r;
		if(rw == 1){ m->io[p] = 1;}
		m->reqtime[p] = clockr();
		sem_post(&(m->c[p].pts));
		sem_wait(&(m->c[p].rs));
	}

	//fill a non-valid page request
	int fillreq(int p, physicalMem * pm){
		int i, x;
		x = -1;
		for(i = 0; i < 256; i++){
			if(pm[i].proc == 0){
				x = i;
				i = 256;
			}
		}
		if(x > -1){
			sem_wait(&(m->c[p].pts));
			if((m->io[p] == 1) && ((clockr() - m->reqtime[p]) < 0.015)){
				sem_post(&(m->c[p].pts));
				return 2;
			}
			pm[x].page = m->reqflg[p];
			if(m->io[p] == 1){
				pm[x].db = 1;
				m->c[p].pt[pm[x].page].db = 1;
			}
			else{
				pm[x].db = 0;
				m->c[p].pt[pm[x].page].db = 0;
			}
			m->reqflg[p] = -1;
			m->io[p] = 0;
			m->reqtime[p] = 0.0;
			pm[x].proc = p;
			pm[x].vb = 1;
			pm[x].tlp = clockr();
			int y = pm[x].page;
			m->c[p].pt[y].p = x;
			m->c[p].pt[y].vb = 1;
			clockw(0.015);
			sem_post(&(m->c[p].pts));
			m->faults += 1;
			sem_post(&(m->c[p].rs));
			return 0;
		}
		else{
			return 1;
		}
	}

	//fill a valid page request	
	int fillvalreq(int p, physicalMem * pm){
		int i, pg, x;
		x = -1;
		sem_wait(&(m->c[p].pts));
		if((m->io[p] == 1) && ((clockr() - m->reqtime[p]) < 0.015)){
			x = 2;
		}
		pg = m->reqflg[p];
		sem_post(&(m->c[p].pts));
		if(x == 2){ return x; }
		for(i = 0; i < 256; i++){
			if((pm[i].proc == p) && (pm[i].page == pg)){
				pm[i].vb = 1;
				pm[i].tlp = clockr();
				sem_wait(&(m->c[p].pts));
				pm[x].db = m->io[p];
				m->c[p].pt[pg].db = pm[x].db;
				m->c[p].pt[pg].vb = 1;
				m->reqflg[p] = -1;
				m->io[p] = 0;
				m->reqtime[p] = 0.0;
				sem_post(&(m->c[p].pts));
				clockw(0.00000001);
				sem_post(&(m->c[p].rs));
				return 0;
			}
		}
		return 1;
	}
