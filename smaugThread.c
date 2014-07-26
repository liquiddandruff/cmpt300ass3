#include <errno.h> 
#include <wait.h> 
#include <stdlib.h> 
#include <stdio.h>
#include <unistd.h>
#include <curses.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/resource.h> 


// Global semaphores
sem_t sem_CowsInGroup;
sem_t mut_CowsInGroup;
sem_t sem_SheepInGroup;
sem_t mut_SheepInGroup;
sem_t sem_SheepWaiting;
sem_t sem_CowsWaiting;
sem_t mut_SheepEaten;
sem_t mut_CowsEaten;
sem_t sem_SheepEaten;
sem_t sem_CowsEaten;
sem_t sem_SheepDead;
sem_t sem_CowsDead;
sem_t mut_Terminate;
sem_t sem_DragonEating;
sem_t sem_DragonSleeping;
sem_t mut_CowMealFlag;
sem_t mut_SheepMealFlag;
sem_t mut_HunterCount;
sem_t sem_HuntersWaiting;
sem_t sem_HunterFinish;
sem_t mut_ThiefCount;
sem_t sem_ThievesWaiting;
sem_t sem_ThiefFinish;

/* System constants used to control simulation termination */
#define MAX_SHEEP_EATEN 36 
#define MAX_COWS_EATEN 12
#define MAX_DEFEATED_HUNTERS 48
#define MAX_DEFEATED_THIEVES 36
#define MAX_COWS_CREATED 80
#define MIN_TREASURE_IN_HOARD 0
#define MAX_TREASURE_IN_HOARD 1000
#define INITIAL_TREASURE_IN_HOARD 500

/* Simulation variables */
#define SECONDS_TO_MICROSECONDS 1000000
// This should be 10 minutes, but that will make the simulation too long. 
// Smaug will nap for 2 seconds, but change to 10*60 = 600 seconds if 10 minutes is actually wanted
#define SMAUG_NAP_LENGTH_US 2*SECONDS_TO_MICROSECONDS
#define JEWELS_FROM_HUNTER_WIN 10
#define JEWELS_FROM_HUNTER_LOSE 5
#define JEWELS_FROM_THIEF_WIN 8
#define JEWELS_FROM_THIEF_LOSE 20

/* System constants to specify size of groups of cows*/
#define SHEEP_IN_GROUP 3
#define COWS_IN_GROUP 1

struct timeval startTime;


/*  Pointers and ids for shared memory segments */
int *terminateFlagp = NULL;
int terminateFlag = 0;

int *sheepMealFlagp = NULL;
int *sheepCounterp = NULL;
int *sheepEatenCounterp = NULL;
int sheepMealFlag = 0;
int sheepCounter = 0;
int sheepEatenCounter = 0;

int *cowMealFlagP = NULL;
int *cowCounterp = NULL;
int *cowsEatenCounterp = NULL;
int cowMealFlag = 0;
int cowCounter = 0;
int cowsEatenCounter = 0;

int *hunterCounterp = NULL;
int hunterCounter = 0;
int *thiefCounterp = NULL;
int thiefCounter = 0;

/* Main processID for output purposes */
int parentProcessID = -1;

double timeChange( struct timeval starttime );
void initialize();
void terminateSimulation();
void releaseSemandMem();

void *smaug(void *smaugWinProbP)
{
	const int smaugWinProb = *(int*)smaugWinProbP;
	int k;
	unsigned long localThreadID = (unsigned long)pthread_self();

	/* local counters used only for smaug routine */
	int numJewels = INITIAL_TREASURE_IN_HOARD;
	int sheepEatenTotal = 0;
	int cowsEatenTotal = 0;
	int thievesDefeatedTotal = 0;
	int huntersDefeatedTotal = 0;
	int sleepThisIteration = 1;
	int terminateNow = 0;
	/* Initialize random number generator*/
	/* Random numbers are used to determine the time between successive beasts */

	printf("SMAUGSMAUGSMAUGSMAUGSMAU   PTHREADID is %lu \n", localThreadID );

	while (terminateNow == 0) {		
		// Smaug goes to sleep if nothing happens and sleepThisIteration is 1
		if(sleepThisIteration == 1) {
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has gone to sleep\n" );
			// We must reset the semaphore to prevent smaug waking up when there's no need to
			int dragonSleepingSemVal;
			sem_getvalue(&sem_DragonSleeping, &dragonSleepingSemVal);
			// Posix semaphores lack the interface for setting the value of a semaphore
			// Since we cannot explicitly set the semaphore to 0 like we did in smaugProcess.c, we 
			// will make it 0 by repeatedly waiting until it is 0.
			while(dragonSleepingSemVal > 0) {
				dragonSleepingSemVal--;
				sem_wait(&sem_DragonSleeping);
			}
			// Now we wait and can be assured that smaug will actually sleep
			sem_wait(&sem_DragonSleeping);
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug sniffs his surroundings\n" );
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has woken up \n" );
		} else {
			sleepThisIteration = 1;
		}

		sem_wait(&mut_ThiefCount);
		sem_wait(&mut_HunterCount);
		if( *hunterCounterp + *thiefCounterp > 0) {
			while( *hunterCounterp + *thiefCounterp > 0 && terminateNow == 0) {
				sem_post(&mut_HunterCount);
				if(*thiefCounterp > 0) {
					*thiefCounterp = *thiefCounterp - 1;
					sem_post(&mut_ThiefCount);
					// Wake thief from wander state for interaction
					sem_post(&sem_ThievesWaiting);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is playing with a thief\n");
					if( rand() % 100 <= smaugWinProb ) {
						thievesDefeatedTotal++;
						numJewels += JEWELS_FROM_THIEF_LOSE;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated a thief (%d thieves have been defeated)\n", thievesDefeatedTotal);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has gained some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_THIEF_LOSE, numJewels);
						if(thievesDefeatedTotal >= MAX_DEFEATED_THIEVES) {
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated %d thieves, so the simulation will terminate.\n", MAX_DEFEATED_THIEVES);
							terminateNow = 1;
							// Shared variable terminateFlagp is only ever SET to 1, not 0 or otherwise, so a mutex lock is unnecessary
							*terminateFlagp = 1;
							break;
						}
					} else {
						numJewels -= JEWELS_FROM_THIEF_WIN;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has been defeated by a thief\n");
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has lost some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_THIEF_WIN, numJewels);
					}
					if( numJewels <= MIN_TREASURE_IN_HOARD || numJewels >= MAX_TREASURE_IN_HOARD) {
						char* condition = numJewels <= MIN_TREASURE_IN_HOARD ? "too less treasure" : "too much treasure";
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has %s, so the simulation will terminate.\n", condition);
						terminateNow = 1;
						*terminateFlagp = 1;
						break;
					}
					sem_post(&sem_ThiefFinish);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a game (1 thief thread has been terminated)\n");
					// Nap and breath
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a nap for %f ms\n", SMAUG_NAP_LENGTH_US/1000.0);
					usleep(SMAUG_NAP_LENGTH_US);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a deep breath\n");
				} else {
					sem_post(&mut_ThiefCount);
					sem_wait(&mut_HunterCount);
					if(*hunterCounterp > 0) {
						*hunterCounterp = *hunterCounterp - 1;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug lifts the spell and allows a hunter to see his cave\n");
						sem_post(&mut_HunterCount);
						// Wake hunter from wander state for interaction
						sem_post(&sem_HuntersWaiting);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is fighting a treasure hunter\n");
						if( rand() % 100 <= smaugWinProb ) {
							huntersDefeatedTotal++;
							numJewels += JEWELS_FROM_HUNTER_LOSE;
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated a treasure hunter (%d hunters have been defeated)\n", huntersDefeatedTotal);
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has gained some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_HUNTER_LOSE, numJewels);
							if(huntersDefeatedTotal >= MAX_DEFEATED_HUNTERS) {
								printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated %d hunters, so the simulation will terminate.\n", MAX_DEFEATED_HUNTERS);
								terminateNow = 1;
								*terminateFlagp = 1;
								break;
							}
						} else {
							numJewels -= JEWELS_FROM_HUNTER_WIN;
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has been defeated by a treasure hunter\n");
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has lost some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_HUNTER_WIN, numJewels);
						}
						if( numJewels <= MIN_TREASURE_IN_HOARD || numJewels >= MAX_TREASURE_IN_HOARD) {
							char* condition = numJewels <= MIN_TREASURE_IN_HOARD ? "too less treasure" : "too much treasure";
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has %s, so the simulation will terminate.\n", condition);
							terminateNow = 1;
							*terminateFlagp = 1;
							break;
						}
						sem_post(&sem_HunterFinish);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a battle (1 treasure hunter thread has been terminated)\n");
						// Nap and breath
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a nap for %f ms\n", SMAUG_NAP_LENGTH_US/1000.0);
						usleep(SMAUG_NAP_LENGTH_US);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a deep breath\n");
					} else {
						sem_post(&mut_HunterCount);
					}
				}
				// Apply protection for next iteration
				sem_wait(&mut_ThiefCount);
				sem_wait(&mut_HunterCount);
			}
			// Release protection
			sem_post(&mut_HunterCount);
			sem_post(&mut_ThiefCount);
		} else {
			// Release protection
			sem_post(&mut_HunterCount);
			sem_post(&mut_ThiefCount);

			// Check animals
			sem_wait(&mut_CowMealFlag);
			sem_wait(&mut_SheepMealFlag);
			// If there's a meal of x cows and y sheeps where x is COWS_IN_GROUP and y is SHEEP_IN_GROUP
			while( *cowMealFlagP >= 1 && *sheepMealFlagp >= 1 && terminateNow == 0) {
				*sheepMealFlagp = *sheepMealFlagp - 1;
				*cowMealFlagP = *cowMealFlagP - 1;
				int mealsLeft = *cowMealFlagP < *sheepMealFlagp ? *cowMealFlagP : *sheepMealFlagp;
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   cow meals: %d sheep meals: %d mealsLeft: %d\n", *cowMealFlagP, *sheepMealFlagp, mealsLeft);
				sem_post(&mut_SheepMealFlag);
				sem_post(&mut_CowMealFlag);
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is eating a meal of %d sheep and %d cow\n", SHEEP_IN_GROUP, COWS_IN_GROUP);
				for( k = 0; k < SHEEP_IN_GROUP; k++ ) {
					sem_post(&sem_SheepWaiting);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   A sheep is ready to eat\n");
				}
				for( k = 0; k < COWS_IN_GROUP; k++ ) {
					sem_post(&sem_CowsWaiting);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   A cow is ready to eat\n");
				}

				/*Smaug waits to eat*/
				sem_wait(&sem_DragonEating);
				for( k = 0; k < SHEEP_IN_GROUP; k++ ) {
					sem_post(&sem_SheepDead);
					sheepEatenTotal++;
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug finished eating a sheep (%d sheep has been eaten)\n", sheepEatenTotal);
				}
				for( k = 0; k < COWS_IN_GROUP; k++ ) {
					sem_post(&sem_CowsDead);
					cowsEatenTotal++;
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug finished eating a cow (%d cows have been eaten)\n", cowsEatenTotal);
				}
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a meal (%d sheep and %d cow threads have been terminated)\n", SHEEP_IN_GROUP, COWS_IN_GROUP);

				// Terminate if terminate conditions are met
				if(sheepEatenTotal >= MAX_SHEEP_EATEN ) {
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has eaten the allowed number of sheep\n");
					terminateNow = 1;
					*terminateFlagp= 1;
					break; 
				}
				if(cowsEatenTotal >= MAX_COWS_EATEN ) {
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has eaten the allowed number of cows\n");
					terminateNow = 1;
					*terminateFlagp= 1;
					break; 
				}


				/* Smaug checks to see if another snack is waiting */
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a deep breath\n");
				sem_wait(&mut_CowMealFlag);
				if( *cowMealFlagP > 0  ) {
					// Mutex check for sheeps is staggered to improve performance, parent else branch is reused for break case
					sem_wait(&mut_SheepMealFlag);
					if(*sheepMealFlagp > 0) {
						sem_wait(&mut_ThiefCount);
						sem_wait(&mut_HunterCount);
						// Check if there are any visitors
						if( *thiefCounterp + *hunterCounterp > 0 ) {
							sem_post(&mut_ThiefCount);
							sem_post(&mut_HunterCount);
							// There are visitors, so don't sleep in the following main iteration and break out of this loop
							sleepThisIteration = 0;	
							break;	
						} else {
							sem_post(&mut_ThiefCount);
							sem_post(&mut_HunterCount);
							// No  visitors, but a meal is waiting, so continue in this loop
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug eats again\n");
							continue;
						}
					} else {
						// Cow and sheep semaphores released after while loop exits
						// Break out of this loop and resume execution of main loop and sleep
						break;
					}
				}
				sem_post(&mut_CowMealFlag);
			} 
			sem_post(&mut_SheepMealFlag);
			sem_post(&mut_CowMealFlag);
		}

	}
	return NULL;
}


void initialize()
{
	/* Init semaphores to 0 */
	sem_init(&sem_SheepInGroup, 0, 0);
	sem_init(&sem_SheepWaiting, 0, 0);
	sem_init(&sem_SheepEaten, 0, 0);
	sem_init(&sem_SheepDead, 0, 0);

	sem_init(&sem_CowsInGroup, 0, 0);
	sem_init(&sem_CowsWaiting, 0, 0);
	sem_init(&sem_CowsEaten, 0, 0);
	sem_init(&sem_CowsDead, 0, 0);

	sem_init(&sem_HuntersWaiting, 0, 0);
	sem_init(&sem_HunterFinish, 0, 0);
	sem_init(&sem_ThievesWaiting, 0, 0);
	sem_init(&sem_ThiefFinish, 0, 0);

	sem_init(&sem_DragonSleeping, 0, 0);
	sem_init(&sem_DragonEating, 0, 0);
	printf("!!INIT!!INIT!!INIT!!  semaphores initiialized\n");
	
	/* Init mutexes to one */
	sem_init(&mut_Terminate, 0, 1);

	sem_init(&mut_SheepMealFlag, 0, 1);
	sem_init(&mut_SheepInGroup, 0, 1);
	sem_init(&mut_SheepEaten, 0, 1);

	sem_init(&mut_CowMealFlag, 0, 1);
	sem_init(&mut_CowsInGroup, 0, 1);
	sem_init(&mut_CowsEaten, 0, 1);

	sem_init(&mut_ThiefCount, 0, 1);
	sem_init(&mut_HunterCount, 0, 1);
	printf("!!INIT!!INIT!!INIT!!  mutexes initiialized\n");


	/* Now we create and attach  the segments of shared memory*/
	if ((terminateFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for terminateFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for terminateFlag\n");
	}
	if ((cowMealFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for cowMealFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for cowMealFlag\n");
	}
	if ((cowCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for cowCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for cowCounter\n");
	}
	if ((cowsEatenCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for cowsEatenCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for cowsEatenCounter\n");
	}
	if ((sheepMealFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for sheepMealFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for sheepMealFlag\n");
	}
	if ((sheepCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for sheepCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for sheepCounter\n");
	}
	if ((sheepEatenCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for sheepEatenCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for sheepEatenCounter\n");
	}
	// hunter
	if ((hunterCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for hunterCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for hunterCounter\n");
	}
	if ((thiefCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for thiefCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for thiefCounter\n");
	}

	/* Now we attach the segment to our data space.  */
	if ((terminateFlagp = shmat(terminateFlag, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for terminateFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for terminateFlag\n");
	}
	if ((cowMealFlagP = shmat(cowMealFlag, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for cowMealFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for cowMealFlag\n");
	}
	if ((cowCounterp = shmat(cowCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for cowCounter\n");
		exit(1);
	} else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for cowCounter\n");
	}
	if ((cowsEatenCounterp = shmat(cowsEatenCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for cowsEatenCounter\n");
		exit(1);
	} else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for cowsEatenCounter\n");
	}
	if ((sheepMealFlagp = shmat(sheepMealFlag, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for sheepMealFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for sheepMealFlag\n");
	}
	if ((sheepCounterp = shmat(sheepCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for sheepCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for sheepCounter\n");
	}
	if ((sheepEatenCounterp = shmat(sheepEatenCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for sheepEatenCounter\n");
		exit(1);
	} else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for sheepEatenCounter\n");
	}
	// hunter
	if ((hunterCounterp = shmat(hunterCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for hunterCounter\n");
		exit(1);
	} else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for hunterCounter\n");
	}
	if ((thiefCounterp = shmat(thiefCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for thiefCounter\n");
		exit(1);
	} else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for thiefCounter\n");
	}

	printf("!!INIT!!INIT!!INIT!!   initialize end\n");
}


void *sheep(void *startTimeNp)
{
	// Cast void* to float*, then dereference float* to get to the actual float value pointed to by the pointer
	float startTimeN = *(float *)startTimeNp;
	// Cast thread id to unsigned long since sometimes the thread id can be very large
	unsigned long localThreadID = (unsigned long)pthread_self();
	int k;

	/* graze */
	printf("SSSSSSS %8lu SSSSSSS   A sheep is born\n", localThreadID);
	if( startTimeN > 0) {
		if( usleep( startTimeN) == -1){
			/* exit when usleep interrupted by kill signal */
			if(errno==EINTR)exit(4);
		}	
	}
	printf("SSSSSSS %8lu SSSSSSS   sheep grazes for %f ms\n", localThreadID, startTimeN);


	/* does this sheep complete a group of SHEEP_IN_GROUP? */
	/* if so wake up the dragon */
	sem_wait(&mut_SheepInGroup);
	sem_post(&sem_SheepInGroup);
	*sheepCounterp = *sheepCounterp + 1;
	printf("SSSSSSS %8lu SSSSSSS   %d  sheeps have been enchanted \n", localThreadID, *sheepCounterp );
	if( ( *sheepCounterp  >= SHEEP_IN_GROUP )) {
		*sheepCounterp = *sheepCounterp - SHEEP_IN_GROUP;
		sem_post(&mut_SheepInGroup);
		for (k=0; k<SHEEP_IN_GROUP; k++){
			sem_wait(&sem_SheepInGroup);
		}
		printf("SSSSSSS %8lu SSSSSSS   The last sheep is waiting\n", localThreadID);
		sem_wait(&mut_SheepMealFlag);
		*sheepMealFlagp = *sheepMealFlagp + 1;
		printf("SSSSSSS %8lu SSSSSSS   signal sheep meal flag %d\n", localThreadID, *sheepMealFlagp);
		sem_post(&mut_SheepMealFlag);

		sem_wait(&mut_CowMealFlag);
		if( *cowMealFlagP >= 1 ) {
			sem_post(&sem_DragonSleeping);
			printf("SSSSSSS %8lu SSSSSSS   last sheep  wakes the dragon \n", localThreadID);
		}
		sem_post(&mut_CowMealFlag);
	}
	else
	{
		sem_post(&mut_SheepInGroup);
	}

	sem_wait(&sem_SheepWaiting);

	// Terminate check
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("SSSSSSS %8lu SSSSSSS   A sheep has been woken up to be eaten after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return;
	} else {
		sem_post(&mut_Terminate);
		printf("SSSSSSS %8lu SSSSSSS   A sheep has been woken up to be eaten\n", localThreadID);
	}

	/* have all the sheeps in group been eaten? */
	/* if so wake up the dragon */
	sem_wait(&mut_SheepEaten);
	sem_post(&sem_SheepEaten);
	*sheepEatenCounterp = *sheepEatenCounterp + 1;
	if( ( *sheepEatenCounterp >= SHEEP_IN_GROUP )) {
		*sheepEatenCounterp = *sheepEatenCounterp - SHEEP_IN_GROUP;
		for (k=0; k<SHEEP_IN_GROUP; k++){
			sem_wait(&sem_SheepEaten);
		}
		printf("SSSSSSS %8lu SSSSSSS   The last sheep has been eaten\n", localThreadID);
		sem_post(&mut_SheepEaten);
		sem_post(&sem_DragonEating);
	}
	else
	{
		sem_post(&mut_SheepEaten);
		printf("SSSSSSS %8lu SSSSSSS   A sheep is waiting to be eaten\n", localThreadID);
	}

	sem_wait(&sem_SheepDead);

	printf("SSSSSSS %8lu SSSSSSS   sheep  dies\n", localThreadID);
	return NULL;
}

void *cow(void *startTimeNp)
{
	float startTimeN = *(float *)startTimeNp;
	unsigned long localThreadID = (unsigned long)pthread_self();
	int k;

	/* graze */
	printf("CCCCCCC %8lu CCCCCCC   A cow is born\n", localThreadID);
	if( startTimeN > 0) {
		if( usleep( startTimeN) == -1){
			/* exit when usleep interrupted by kill signal */
			if(errno==EINTR)exit(4);
		}	
	}
	printf("CCCCCCC %8lu CCCCCCC   cow grazes for %f ms\n", localThreadID, startTimeN);

	/* does this cow complete a group of COWS_IN_GROUP? */
	/* if so wake up the dragon */
	sem_wait(&mut_CowsInGroup);
	sem_post(&sem_CowsInGroup);
	*cowCounterp = *cowCounterp + 1;
	printf("CCCCCCC %8lu CCCCCCC   %d  cow has been enchanted \n", localThreadID, *cowCounterp );
	if( ( *cowCounterp  >= COWS_IN_GROUP )) {
		*cowCounterp = *cowCounterp - COWS_IN_GROUP;
		sem_post(&mut_CowsInGroup);
		for (k=0; k<COWS_IN_GROUP; k++){
			sem_wait(&sem_CowsInGroup);
		}
		printf("CCCCCCC %8lu CCCCCCC   The last cow is waiting\n", localThreadID);
		sem_wait(&mut_CowMealFlag);
		*cowMealFlagP = *cowMealFlagP + 1;
		printf("CCCCCCC %8lu CCCCCCC   signal cow meal flag %d\n", localThreadID, *cowMealFlagP);
		sem_post(&mut_CowMealFlag);

		sem_wait(&mut_SheepMealFlag);
		if( *sheepMealFlagp >= 1 ) {
			sem_post(&sem_DragonEating);
			printf("CCCCCCC %8lu CCCCCCC   last cow  wakes the dragon \n", localThreadID);
		}	
		sem_post(&mut_SheepMealFlag);
	}
	else
	{
		sem_post(&mut_CowsInGroup);
	}

	sem_wait(&sem_CowsWaiting);

	// Terminate check
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("CCCCCCC %8lu CCCCCCC   A cow has been woken up to be eaten after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return;
	} else {
		sem_post(&mut_Terminate);
		printf("CCCCCCC %8lu CCCCCCC   A cow has been woken up to be eaten\n", localThreadID);
	}

	/* have all the cows in group been eaten? */
	/* if so wake up the dragon */
	sem_wait(&mut_CowsEaten);
	sem_post(&sem_CowsEaten);
	*cowsEatenCounterp = *cowsEatenCounterp + 1;
	if( ( *cowsEatenCounterp >= COWS_IN_GROUP )) {
		*cowsEatenCounterp = *cowsEatenCounterp - COWS_IN_GROUP;
		for (k=0; k<COWS_IN_GROUP; k++){
			sem_wait(&sem_CowsEaten);
		}
		printf("CCCCCCC %8lu CCCCCCC   The last cow has been eaten\n", localThreadID);
		sem_post(&mut_CowsEaten);
		sem_post(&sem_DragonEating);
	}
	else
	{
		sem_post(&mut_CowsEaten);
		printf("CCCCCCC %8lu CCCCCCC   A cow is waiting to be eaten\n", localThreadID);
	}
	sem_wait(&sem_CowsDead);

	printf("CCCCCCC %8lu CCCCCCC   cow  dies\n", localThreadID);

	return NULL;
}

void *thief(void *startTimeNp)
{
	float startTimeN = *(float *)startTimeNp;
	unsigned long localThreadID = (unsigned long)pthread_self();
    
    printf("TTTTTTT %8lu TTTTTTT   A thief arrived outside the valley\n", localThreadID);
	if( startTimeN > 0) {
		if( usleep( startTimeN) == -1){
			/* exit when usleep interrupted by kill signal */
			if(errno==EINTR)exit(4);
		}	
	}

	// Terminate check
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("TTTTTTT %8lu TTTTTTT   thief has found the magical path after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return NULL;
	} else {
		printf("TTTTTTT %8lu TTTTTTT   thief has found the magical path in %f ms\n", localThreadID, startTimeN);
		sem_post(&mut_Terminate);
	}

	sem_wait(&mut_ThiefCount);
	*thiefCounterp = *thiefCounterp + 1;
	sem_post(&mut_ThiefCount);
	printf("TTTTTTT %8lu TTTTTTT   thief is under smaug's spell and is waiting to be interacted with\n", localThreadID);
	printf("TTTTTTT %8lu TTTTTTT   thief wakes smaug\n", localThreadID);
	sem_post(&sem_DragonSleeping);
	sem_wait(&sem_ThievesWaiting);
	// Another terminate check incase this thief ends the simulation and the semaphores are cleaned up
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("TTTTTTT %8lu TTTTTTT   thief enters smaug's cave after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return NULL;
	} else {
		printf("TTTTTTT %8lu TTTTTTT   thief enters smaug's cave\n", localThreadID);
		printf("TTTTTTT %8lu TTTTTTT   thief plays with smaug\n", localThreadID);
		sem_post(&mut_Terminate);
	}
	sem_wait(&sem_ThiefFinish);
	printf("TTTTTTT %8lu TTTTTTT   thief leaves cave and goes home\n", localThreadID);

	return NULL;
}

void *hunter(void *startTimeNp)
{
	float startTimeN = *(float *)startTimeNp;
	unsigned long localThreadID = (unsigned long)pthread_self();
    
    printf("HHHHHHH %8lu HHHHHHH   A hunter arrived outside the valley\n", localThreadID);
	if( startTimeN > 0) {
		if( usleep( startTimeN) == -1){
			/* exit when usleep interrupted by kill signal */
			if(errno==EINTR)exit(4);
		}	
	}

	// Terminate check
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("HHHHHHH %8lu HHHHHHH   hunter has found the magical path after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return NULL;
	} else {
		printf("HHHHHHH %8lu HHHHHHH   hunter has found the magical path in %f ms\n", localThreadID, startTimeN);
		sem_post(&mut_Terminate);
	}

	sem_wait(&mut_HunterCount);
	*hunterCounterp = *hunterCounterp + 1;
	sem_post(&mut_HunterCount);
	printf("HHHHHHH %8lu HHHHHHH   hunter is under smaug's spell and is waiting to be interacted with\n", localThreadID);
	printf("HHHHHHH %8lu HHHHHHH   hunter wakes smaug\n", localThreadID);
	sem_post(&sem_DragonSleeping);
	sem_wait(&sem_HuntersWaiting);
	// Another terminate check incase this hunter ends the simulation and the semaphores are cleaned up
	sem_wait(&mut_Terminate);
	if( *terminateFlagp == 1 ) {
		printf("HHHHHHH %8lu HHHHHHH   hunter enters smaug's cave after we've been told to terminate\n", localThreadID);
		sem_post(&mut_Terminate);
		return NULL;
	} else {
		printf("HHHHHHH %8lu HHHHHHH   hunter enters smaug's cave\n", localThreadID);
		printf("HHHHHHH %8lu HHHHHHH   hunter fights smaug\n", localThreadID);
		sem_post(&mut_Terminate);
	}
	sem_wait(&sem_HunterFinish);
	printf("TTTTTTT %8lu TTTTTTT   hunter leaves cave and goes home\n", localThreadID);

	return NULL;
}


void terminateSimulation() {
	pid_t localpid;

	localpid = getpid();
	printf("RELEASESEMAPHORES   Terminating Simulation from process: %8d threadid: %8lu\n", localpid, (unsigned long)pthread_self());

	// Child threads spawned by the main thread will terminate automatically when main thread terminates
	// so not much cleanup to do here
	printf("XXTERMINATETERMINATE   sheep threads terminating\n");
	printf("XXTERMINATETERMINATE   cow threads terminating\n");
	printf("XXTERMINATETERMINATE   hunter threads terminating\n");
	printf("XXTERMINATETERMINATE   thief threads terminating\n");
	printf("XXTERMINATETERMINATE   smaug thread terminating\n");

	// No child processes are created in this threaded version, so no need to waitpid -1 them

	releaseSemandMem();

	printf("GOODBYE from terminate\n");
}

void destroySemCheck(int ret, char *semName) {
	if(ret == 0) {
		printf("SEMAPHORE RELEASERELEASERELEAS   Successfully destroyed semaphore %s\n", semName);
	} else {
		printf("SEMAPHORE RELEASERELEASERELEAS   Error destroying semaphore %s\n", semName);
	}
}

void releaseSemandMem() 
{
	// Wait for the semaphores, especially for the terminate semaphore to allow the threads to 
	// terminate gracefully

	printf("RELEASERELEASERELEAS   Sleeping for one second to allow threads to terminate gracefully\n");
	sleep(1);

	destroySemCheck(sem_destroy(&sem_SheepInGroup)   , "sem_SheepInGroup");
	destroySemCheck(sem_destroy(&sem_SheepWaiting)   , "sem_SheepWaiting");
	destroySemCheck(sem_destroy(&sem_SheepEaten)     , "sem_SheepEaten");
	destroySemCheck(sem_destroy(&sem_SheepDead)      , "sem_SheepDead");
	destroySemCheck(sem_destroy(&mut_SheepInGroup)   , "mut_SheepInGroup");
	destroySemCheck(sem_destroy(&mut_SheepEaten)     , "mut_SheepEaten");
	destroySemCheck(sem_destroy(&mut_SheepMealFlag)  , "mut_SheepMealFlag");

	destroySemCheck(sem_destroy(&sem_CowsInGroup)    , "sem_CowsInGroup");
	destroySemCheck(sem_destroy(&sem_CowsWaiting)    , "sem_CowsWaiting");
	destroySemCheck(sem_destroy(&sem_CowsEaten)      , "sem_CowsEaten");
	destroySemCheck(sem_destroy(&sem_CowsDead)       , "sem_CowsDead");
	destroySemCheck(sem_destroy(&mut_CowsInGroup)    , "mut_CowsInGroup");
	destroySemCheck(sem_destroy(&mut_CowsEaten)      , "mut_CowsEaten");
	destroySemCheck(sem_destroy(&mut_CowMealFlag)    , "mut_CowMealFlag");

	destroySemCheck(sem_destroy(&mut_HunterCount)    , "mut_HunterCount");
	destroySemCheck(sem_destroy(&sem_HuntersWaiting) , "sem_HuntersWaiting");
	destroySemCheck(sem_destroy(&sem_HunterFinish)   , "sem_HunterFinish");

	destroySemCheck(sem_destroy(&mut_ThiefCount)     , "mut_ThiefCount");
	destroySemCheck(sem_destroy(&sem_ThievesWaiting) , "sem_ThievesWaiting");
	destroySemCheck(sem_destroy(&sem_ThiefFinish)    , "sem_ThiefFinish");

	destroySemCheck(sem_destroy(&sem_DragonEating)   , "sem_DragonEating");
	destroySemCheck(sem_destroy(&sem_DragonSleeping) , "sem_DragonSleeping");
	destroySemCheck(sem_destroy(&mut_Terminate)      , "mut_Terminate");

	if(shmdt(terminateFlagp)==-1) {
		printf("RELEASERELEASERELEAS   terminateFlagp shared memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   terminateFlagp shared memory detached\n");
	}
	if( shmctl(terminateFlag, IPC_RMID, NULL ))
	{
		// this will dereferrence the null pointer which has just been freed above; remove it
		printf("RELEASERELEASERELEAS   terminateFlag shared memory delete failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   terminateFlag shared memory deleted\n");
	}
	// SHEEP MEMORY
	if( shmdt(sheepMealFlagp)==-1)
	{
		printf("RELEASERELEASERELEAS   sheepMealFlagp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepMealFlagp memory detached\n");
	}
	if( shmctl(sheepMealFlag, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   sheepMealFlag shared memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepMealFlag shared memory deleted\n");
	}
	if( shmdt(sheepCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   sheepCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepCounterp memory detached\n");
	}
	if( shmctl(sheepCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   sheepCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepCounter memory deleted\n");
	}
	if( shmdt(sheepEatenCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   sheepEatenCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepEatenCounterp memory detached\n");
	}
	if( shmctl(sheepEatenCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   sheepEatenCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   sheepEatenCounter memory deleted\n");
	}
	// COW MEMORY
	if( shmdt(cowMealFlagP)==-1)
	{
		printf("RELEASERELEASERELEAS   cowMealFlagP memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowMealFlagP memory detached\n");
	}
	if( shmctl(cowMealFlag, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   cowMealFlag shared memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowMealFlag shared memory deleted\n");
	}
	if( shmdt(cowCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   cowCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowCounterp memory detached\n");
	}
	if( shmctl(cowCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   cowCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowCounter memory deleted\n");
	}
	if( shmdt(cowsEatenCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   cowsEatenCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowsEatenCounterp memory detached\n");
	}
	if( shmctl(cowsEatenCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   cowsEatenCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   cowsEatenCounter memory deleted\n");
	}
	// HUNTER MEMORY
	if( shmdt(hunterCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   hunterCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   hunterCounterp memory detached\n");
	}
	if( shmctl(hunterCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   hunterCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   hunterCounter memory deleted\n");
	}
	// THIEF MEMORY
	if( shmdt(thiefCounterp)==-1)
	{
		printf("RELEASERELEASERELEAS   thiefCounterp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   thiefCounterp memory detached\n");
	}
	if( shmctl(thiefCounter, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   thiefCounter memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   thiefCounter memory deleted\n");
	}
}
double timeChange( const struct timeval startTime )
{
	struct timeval nowTime;
	double elapsedTime;

	gettimeofday(&nowTime,NULL);
	elapsedTime = (nowTime.tv_sec - startTime.tv_sec)*1000.0;
	elapsedTime +=  (nowTime.tv_usec - startTime.tv_usec)/1000.0;
	return elapsedTime;

}

int getInputFor(char *prompt);

int main() {
	initialize();
	printf("Main threadid: %lu\n", (unsigned long)pthread_self());
	printf("1s (1 second) is 1000000us (1e6 microseconds)\n");
	const int seed = getInputFor("the seed");
	const int maximumSheepInterval = getInputFor("maximumSheepInterval (us)");
	const int maximumCowInterval = getInputFor("maximumCowInterval (us)");
	const int maximumHunterInterval = getInputFor("maximumHunterInterval (us)");
	const int maximumThiefInterval = getInputFor("maximumThiefInterval (us)");
	const int smaugWinProb = getInputFor("smaugWinProb (0 to 100)");

	double sheepTimer = 0;
	double cowTimer = 0;
	double hunterTimer = 0;
	double thiefTimer = 0;

	srand(seed);
	parentProcessID = getpid();

	pthread_t smaugThread;
	// If pthraed_create returns anything that's not a zero, then we will progress 
	// into the if body and handle the error by terminating
	if(pthread_create(&smaugThread, NULL, smaug, &smaugWinProb)) {
		printf("error creating thread!\n");
		terminateSimulation();
		return 1;
	}
	pthread_detach(smaugThread);
		
	gettimeofday(&startTime, NULL);
	while(*terminateFlagp == 0) {
		double simDuration = timeChange(startTime);

		if(sheepTimer - simDuration <= 0) {
			sheepTimer = simDuration + (rand() % maximumSheepInterval) / 1000.0;
			printf("SHEEP CREATED! next sheep at: %f\n", sheepTimer);
			float sleepTime = (rand() % maximumSheepInterval) / 1000.0;
			pthread_t sheepThread;
			if(pthread_create(&sheepThread, NULL, sheep, &sleepTime)) {
				// We have ran out of memory/hit max number of avail threads/got hit by cosmic rays, so
				// abort and terminate.
				printf("Error creating sheep thread!\n"); 
				terminateSimulation();
				return 1;
			}
			// To free up resources, we would need to join our threads upon completion
			// However, we would not be returning anything important anyways, so just detach our threads
			pthread_detach(sheepThread);
		}

		if(cowTimer - simDuration <= 0) {
			cowTimer = simDuration + (rand() % maximumCowInterval) / 1000.0;
			printf("COW CREATED! next cow at: %f\n", cowTimer);
			float cowTime = (rand() % maximumCowInterval) / 1000.0;
			pthread_t cowThread;
			if(pthread_create(&cowThread, NULL, cow, &cowTime)) {
				printf("Error creating cow thread!\n"); 
				terminateSimulation();
				return 1;
			}
			pthread_detach(cowThread);
		}

		if(thiefTimer - simDuration <= 0) {
			thiefTimer = simDuration + (rand() % maximumThiefInterval) / 1000.0;
			printf("THIEF CREATED! next thief at: %f\n", thiefTimer);
			float thiefTime = (rand() % maximumThiefInterval) / 1000.0;
			pthread_t thiefThread;
			if(pthread_create(&thiefThread, NULL, thief, &thiefTime)) { 
				printf("Error creating thief thread!\n"); 
				terminateSimulation();
				return 1;
			}
			pthread_detach(thiefThread);
		}

		if(hunterTimer - simDuration <= 0) {
			hunterTimer = simDuration + (rand() % maximumHunterInterval) / 1000.0;
			printf("HUNTER CREATED! next hunter at: %f\n", hunterTimer);
			float hunterTime = (rand() % maximumHunterInterval) / 1000.0;
			pthread_t hunterThread;
			if(pthread_create(&hunterThread, NULL, hunter, &hunterTime)) {
				printf("Error creating hunter thread!\n"); 
				terminateSimulation();
				return 1;
			}
			pthread_detach(hunterThread);
		} 
	}
	
	terminateSimulation();
	return 0;
}

int getInputFor(char *prompt) {
	printf("Enter the value for %s: ", prompt);
	int input = 0;
	scanf("%d", &input);
	return input;
}

