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
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/resource.h> 


/* Define semaphores to be placed in a single semaphore set */
/* Numbers indicate index in semaphore set for named semaphore */
#define SEM_COWSINGROUP 0
#define SEM_PCOWSINGROUP 1
#define SEM_SHEEPINGROUP 2
#define SEM_PSHEEPINGROUP 3
#define SEM_SHEEPWAITING 7
#define SEM_COWSWAITING 8
#define SEM_PSHEEPEATEN 10
#define SEM_PCOWSEATEN 11
#define SEM_SHEEPEATEN 12
#define SEM_COWSEATEN 13
#define SEM_SHEEPDEAD 15
#define SEM_COWSDEAD 16
#define SEM_PTERMINATE 17
#define SEM_DRAGONEATING 19
#define SEM_DRAGONFIGHTING 20
#define SEM_DRAGONSLEEPING 21
#define SEM_PCOWMEALFLAG 23
#define SEM_PSHEEPMEALFLAG 24

#define SEM_PHUNTERCOUNT 26
#define SEM_HUNTERSWAITING 27
#define SEM_HUNTERFINISH 28

#define SEM_PTHIEFCOUNT 29
#define SEM_THIEVESWAITING 30
#define SEM_THIEFFINISH 31

#define MAX_SEMAPHORES 32

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
#define SMAUG_NAP_LENGTH_US 2*SECONDS_TO_MICROSECONDS
#define JEWELS_FROM_HUNTER_WIN 10
#define JEWELS_FROM_HUNTER_LOSE 5
#define JEWELS_FROM_THIEF_WIN 8
#define JEWELS_FROM_THIEF_LOSE 20

/* System constants to specify size of groups of cows*/
#define SHEEP_IN_GROUP 3
#define COWS_IN_GROUP 1

/* CREATING YOUR SEMAPHORES */
int semID; 

union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
} seminfo;

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

/* Group IDs for managing/removing processes */
int parentProcessID = -1;
int smaugProcessID = -1;
int sheepProcessGID = -1;
int cowProcessGID = -1;
int hunterProcessGID = -1;
int thiefProcessGID = -1;


/* Define the semaphore operations for each semaphore */
/* Arguments of each definition are: */
/* Name of semaphore on which the operation is done */
/* Increment (amount added to the semaphore when operation executes*/
/* Flag values (block when semaphore <0, enable undo ...)*/

/*Number in group semaphores*/
struct sembuf WaitSheepInGroup={SEM_SHEEPINGROUP, -1, 0};
struct sembuf SignalSheepInGroup={SEM_SHEEPINGROUP, 1, 0};
struct sembuf WaitCowsInGroup={SEM_COWSINGROUP, -1, 0};
struct sembuf SignalCowsInGroup={SEM_COWSINGROUP, 1, 0};

/*Number in group mutexes*/
struct sembuf WaitProtectSheepMealFlag={SEM_PSHEEPMEALFLAG, -1, 0};
struct sembuf SignalProtectSheepMealFlag={SEM_PSHEEPMEALFLAG, 1, 0};
struct sembuf WaitProtectSheepInGroup={SEM_PSHEEPINGROUP, -1, 0};
struct sembuf SignalProtectSheepInGroup={SEM_PSHEEPINGROUP, 1, 0};

struct sembuf WaitProtectCowMealFlag={SEM_PCOWMEALFLAG, -1, 0};
struct sembuf SignalProtectCowMealFlag={SEM_PCOWMEALFLAG, 1, 0};
struct sembuf WaitProtectCowsInGroup={SEM_PCOWSINGROUP, -1, 0};
struct sembuf SignalProtectCowsInGroup={SEM_PCOWSINGROUP, 1, 0};

struct sembuf WaitProtectThiefCount={SEM_PTHIEFCOUNT, -1, 0};
struct sembuf SignalProtectThiefCount={SEM_PTHIEFCOUNT, 1, 0};
struct sembuf WaitProtectHunterCount={SEM_PHUNTERCOUNT, -1, 0};
struct sembuf SignalProtectHunterCount={SEM_PHUNTERCOUNT, 1, 0};

/*Number waiting sempahores*/
struct sembuf WaitSheepWaiting={SEM_SHEEPWAITING, -1, 0};
struct sembuf SignalSheepWaiting={SEM_SHEEPWAITING, 1, 0};
struct sembuf WaitCowsWaiting={SEM_COWSWAITING, -1, 0};
struct sembuf SignalCowsWaiting={SEM_COWSWAITING, 1, 0};

struct sembuf WaitHuntersWaiting={SEM_HUNTERSWAITING, -1, 0};
struct sembuf SignalHuntersWaiting={SEM_HUNTERSWAITING, 1, 0};
struct sembuf WaitHunterFinish={SEM_HUNTERFINISH, -1, 0};
struct sembuf SignalHunterFinish={SEM_HUNTERFINISH, 1, 0};

struct sembuf WaitThievesWaiting={SEM_THIEVESWAITING, -1, 0};
struct sembuf SignalThievesWaiting={SEM_THIEVESWAITING, 1, 0};
struct sembuf WaitThiefFinish={SEM_THIEFFINISH, -1, 0};
struct sembuf SignalThiefFinish={SEM_THIEFFINISH, 1, 0};

/*Number eaten or fought semaphores*/
struct sembuf WaitSheepEaten={SEM_SHEEPEATEN, -1, 0};
struct sembuf SignalSheepEaten={SEM_SHEEPEATEN, 1, 0};
struct sembuf WaitCowsEaten={SEM_COWSEATEN, -1, 0};
struct sembuf SignalCowsEaten={SEM_COWSEATEN, 1, 0};

/*Number eaten or fought mutexes*/
struct sembuf WaitProtectSheepEaten={SEM_PSHEEPEATEN, -1, 0};
struct sembuf SignalProtectSheepEaten={SEM_PSHEEPEATEN, 1, 0};
struct sembuf WaitProtectCowsEaten={SEM_PCOWSEATEN, -1, 0};
struct sembuf SignalProtectCowsEaten={SEM_PCOWSEATEN, 1, 0};

/*Number Dead semaphores*/
struct sembuf WaitSheepDead={SEM_SHEEPDEAD, -1, 0};
struct sembuf SignalSheepDead={SEM_SHEEPDEAD, 1, 0};
struct sembuf WaitCowsDead={SEM_COWSDEAD, -1, 0};
struct sembuf SignalCowsDead={SEM_COWSDEAD, 1, 0};

/*Dragon Semaphores*/
struct sembuf WaitDragonEating={SEM_DRAGONEATING, -1, 0};
struct sembuf SignalDragonEating={SEM_DRAGONEATING, 1, 0};
struct sembuf WaitDragonFighting={SEM_DRAGONFIGHTING, -1, 0};
struct sembuf SignalDragonFighting={SEM_DRAGONFIGHTING, 1, 0};
struct sembuf WaitDragonSleeping={SEM_DRAGONSLEEPING, -1, 0};
struct sembuf SignalDragonSleeping={SEM_DRAGONSLEEPING, 1, 0};

/*Termination Mutex*/
struct sembuf WaitProtectTerminate={SEM_PTERMINATE, -1, 0};
struct sembuf SignalProtectTerminate={SEM_PTERMINATE, 1, 0};


double timeChange( struct timeval starttime );
void initialize();
void terminateSimulation();
void releaseSemandMem();
void semopChecked(int semaphoreID, struct sembuf *operation, unsigned something); 
void semctlChecked(int semaphoreID, int semNum, int flag, union semun seminfo); 


void *smaug(void *smaugWinProbP)
{
	const int smaugWinProb = *(int*)smaugWinProbP;
	int k;
	unsigned int localThreadID = (unsigned int)pthread_self();

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
			semopChecked(semID, &WaitDragonSleeping, 1);
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug sniffs his surroundings\n" );
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has woken up \n" );
		} else {
			sleepThisIteration = 1;
		}

		semopChecked(semID, &WaitProtectThiefCount, 1);
		semopChecked(semID, &WaitProtectHunterCount, 1);
		if( *hunterCounterp + *thiefCounterp > 0) {
			while( *hunterCounterp + *thiefCounterp > 0 && terminateNow == 0) {
				semopChecked(semID, &SignalProtectHunterCount, 1);
				if(*thiefCounterp > 0) {
					*thiefCounterp = *thiefCounterp - 1;
					semopChecked(semID, &SignalProtectThiefCount, 1);
					// Wake thief from wander state for interaction
					semopChecked(semID, &SignalThievesWaiting, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is playing with a thief\n");
					if( rand() % 100 <= smaugWinProb ) {
						thievesDefeatedTotal++;
						numJewels += JEWELS_FROM_THIEF_LOSE;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated a thief (%d thieves have been defeated)\n", thievesDefeatedTotal);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has gained some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_THIEF_LOSE, numJewels);
						if(thievesDefeatedTotal >= MAX_DEFEATED_THIEVES) {
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has defeated %d thieves, so the simulation will terminate.\n", MAX_DEFEATED_THIEVES);
							terminateNow = 1;
							*terminateFlagp = 1;
							break;
						}
					} else {
						numJewels -= JEWELS_FROM_THIEF_WIN;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has been defeated by a thief\n");
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has lost some treasure (%d jewels). He now has %d jewels.\n", JEWELS_FROM_THIEF_WIN, numJewels);
					}
					if( numJewels < MIN_TREASURE_IN_HOARD || numJewels > MAX_TREASURE_IN_HOARD) {
						char* condition = numJewels < MIN_TREASURE_IN_HOARD ? "too less treasure" : "too much treasure";
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has %s, so the simulation will terminate.\n", condition);
						terminateNow = 1;
						*terminateFlagp = 1;
						break;
					}
					semopChecked(semID, &SignalThiefFinish, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a game (1 thief process has been terminated)\n");
					// Nap and breath
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a nap for %f ms\n", SMAUG_NAP_LENGTH_US/1000.0);
					usleep(SMAUG_NAP_LENGTH_US);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a deep breath\n");
				} else {
					semopChecked(semID, &SignalProtectThiefCount, 1);
					semopChecked(semID, &WaitProtectHunterCount, 1);
					if(*hunterCounterp > 0) {
						*hunterCounterp = *hunterCounterp - 1;
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug lifts the spell and allows a hunter to see his cave\n");
						semopChecked(semID, &SignalProtectHunterCount, 1);
						// Wake hunter from wander state for interaction
						semopChecked(semID, &SignalHuntersWaiting, 1);
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
						if( numJewels < MIN_TREASURE_IN_HOARD || numJewels > MAX_TREASURE_IN_HOARD) {
							char* condition = numJewels < MIN_TREASURE_IN_HOARD ? "too less treasure" : "too much treasure";
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has %s, so the simulation will terminate.\n", condition);
							terminateNow = 1;
							*terminateFlagp = 1;
							break;
						}
						semopChecked(semID, &SignalHunterFinish, 1);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a battle (1 treasure hunter process has been terminated)\n");
						// Nap and breath
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a nap for %f ms\n", SMAUG_NAP_LENGTH_US/1000.0);
						usleep(SMAUG_NAP_LENGTH_US);
						printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug takes a deep breath\n");
					} else {
						semopChecked(semID, &SignalProtectHunterCount, 1);
					}
				}
				// Apply protection for next iteration
				semopChecked(semID, &WaitProtectThiefCount, 1);
				semopChecked(semID, &WaitProtectHunterCount, 1);
			}
			// Release protection
			semopChecked(semID, &SignalProtectHunterCount, 1);
			semopChecked(semID, &SignalProtectThiefCount, 1);
		} else {
			// Release protection
			semopChecked(semID, &SignalProtectHunterCount, 1);
			semopChecked(semID, &SignalProtectThiefCount, 1);

			// Check animals
			semopChecked(semID, &WaitProtectCowMealFlag, 1);
			semopChecked(semID, &WaitProtectSheepMealFlag, 1);
			// If there's a meal of x cows and y sheeps where x is COWS_IN_GROUP and y is SHEEP_IN_GROUP
			while( *cowMealFlagP >= 1 && *sheepMealFlagp >= 1 && terminateNow == 0) {
				*sheepMealFlagp = *sheepMealFlagp - 1;
				*cowMealFlagP = *cowMealFlagP - 1;
				int mealsLeft = *cowMealFlagP < *sheepMealFlagp ? *cowMealFlagP : *sheepMealFlagp;
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   cow meals: %d sheep meals: %d mealsLeft: %d\n", *cowMealFlagP, *sheepMealFlagp, mealsLeft);
				semopChecked(semID, &SignalProtectSheepMealFlag, 1);
				semopChecked(semID, &SignalProtectCowMealFlag, 1);
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is eating a meal of %d sheep and %d cow\n", SHEEP_IN_GROUP, COWS_IN_GROUP);
				for( k = 0; k < SHEEP_IN_GROUP; k++ ) {
					semopChecked(semID, &SignalSheepWaiting, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   A sheep is ready to eat\n");
				}
				for( k = 0; k < COWS_IN_GROUP; k++ ) {
					semopChecked(semID, &SignalCowsWaiting, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   A cow is ready to eat\n");
				}

				/*Smaug waits to eat*/
				semopChecked(semID, &WaitDragonEating, 1);
				for( k = 0; k < SHEEP_IN_GROUP; k++ ) {
					semopChecked(semID, &SignalSheepDead, 1);
					sheepEatenTotal++;
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug finished eating a sheep (%d sheep has been eaten)\n", sheepEatenTotal);
				}
				for( k = 0; k < COWS_IN_GROUP; k++ ) {
					semopChecked(semID, &SignalCowsDead, 1);
					cowsEatenTotal++;
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug finished eating a cow (%d cows have been eaten)\n", cowsEatenTotal);
				}
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a meal (%d sheep and %d cow process has been terminated)\n", SHEEP_IN_GROUP, COWS_IN_GROUP);

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
				semopChecked(semID, &WaitProtectCowMealFlag, 1);
				if( *cowMealFlagP > 0  ) {
					// Mutex check for sheeps is staggered to improve performance, parent else branch is reused for break case
					semopChecked(semID, &WaitProtectSheepMealFlag, 1);
					if(*sheepMealFlagp > 0) {
						semopChecked(semID, &WaitProtectThiefCount, 1);
						semopChecked(semID, &WaitProtectHunterCount, 1);
						// Check if there are any visitors
						if( *thiefCounterp + *hunterCounterp > 0 ) {
							semopChecked(semID, &SignalProtectThiefCount, 1);
							semopChecked(semID, &SignalProtectHunterCount, 1);
							semopChecked(semID, &SignalProtectCowMealFlag, 1);
							semopChecked(semID, &SignalProtectSheepMealFlag, 1);
							// There are visitors, so don't sleep in the following main iteration and break out of this loop
							sleepThisIteration = 0;	
							break;	
						} else {
							semopChecked(semID, &SignalProtectThiefCount, 1);
							semopChecked(semID, &SignalProtectHunterCount, 1);
							// No  visitors, but a meal is waiting, so continue in this loop
							printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug eats again\n", localThreadID);
							continue;
						}
					} else {
						semopChecked(semID, &SignalProtectCowMealFlag, 1);
						semopChecked(semID, &SignalProtectSheepMealFlag, 1);
						// Break out of this loop and resume execution of main loop and sleep
						break;
					}
				}
				else {
					semopChecked(semID, &SignalProtectCowMealFlag, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug sleeps again\n", localThreadID);
					semopChecked(semID, &WaitDragonSleeping, 1);
					printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is awake again\n", localThreadID);
					break;
				}
			} /*else {
				// Resume execution at beginning of loop and sleep
				semopChecked(semID, &SignalProtectSheepMealFlag, 1);
				semopChecked(semID, &SignalProtectCowMealFlag, 1);
				continue;
			}*/
			semopChecked(semID, &SignalProtectSheepMealFlag, 1);
			semopChecked(semID, &SignalProtectCowMealFlag, 1);
		}

	}
	return NULL;
}


void initialize()
{
	/* Init semaphores */
	semID=semget(IPC_PRIVATE, MAX_SEMAPHORES, 0666 | IPC_CREAT);


	/* Init to zero, no elements are produced yet */
	seminfo.val=0;
	semctlChecked(semID, SEM_SHEEPINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_SHEEPWAITING, SETVAL, seminfo);
	semctlChecked(semID, SEM_SHEEPEATEN, SETVAL, seminfo);
	semctlChecked(semID, SEM_SHEEPDEAD, SETVAL, seminfo);

	semctlChecked(semID, SEM_COWSINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSWAITING, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSEATEN, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSDEAD, SETVAL, seminfo);

	semctlChecked(semID, SEM_HUNTERSWAITING, SETVAL, seminfo);
	semctlChecked(semID, SEM_HUNTERFINISH, SETVAL, seminfo);
	semctlChecked(semID, SEM_THIEVESWAITING, SETVAL, seminfo);
	semctlChecked(semID, SEM_THIEFFINISH, SETVAL, seminfo);

	semctlChecked(semID, SEM_DRAGONFIGHTING, SETVAL, seminfo);
	semctlChecked(semID, SEM_DRAGONSLEEPING, SETVAL, seminfo);
	semctlChecked(semID, SEM_DRAGONEATING, SETVAL, seminfo);
	printf("!!INIT!!INIT!!INIT!!  semaphores initiialized\n");
	
	/* Init Mutex to one */
	seminfo.val=1;
	semctlChecked(semID, SEM_PTERMINATE, SETVAL, seminfo);

	semctlChecked(semID, SEM_PSHEEPMEALFLAG, SETVAL, seminfo);
	semctlChecked(semID, SEM_PSHEEPINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_PSHEEPEATEN, SETVAL, seminfo);

	semctlChecked(semID, SEM_PCOWMEALFLAG, SETVAL, seminfo);
	semctlChecked(semID, SEM_PCOWSINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_PCOWSEATEN, SETVAL, seminfo);

	semctlChecked(semID, SEM_PTHIEFCOUNT, SETVAL, seminfo);
	semctlChecked(semID, SEM_PHUNTERCOUNT, SETVAL, seminfo);
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
	float startTimeN = *(float *)startTimeNp;
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
	semopChecked(semID, &WaitProtectSheepInGroup, 1);
	semopChecked(semID, &SignalSheepInGroup, 1);
	*sheepCounterp = *sheepCounterp + 1;
	printf("SSSSSSS %8lu SSSSSSS   %d  sheeps have been enchanted \n", localThreadID, *sheepCounterp );
	if( ( *sheepCounterp  >= SHEEP_IN_GROUP )) {
		*sheepCounterp = *sheepCounterp - SHEEP_IN_GROUP;
		semopChecked(semID, &SignalProtectSheepInGroup, 1);
		for (k=0; k<SHEEP_IN_GROUP; k++){
			semopChecked(semID, &WaitSheepInGroup, 1);
		}
		printf("SSSSSSS %8lu SSSSSSS   The last sheep is waiting\n", localThreadID);
		semopChecked(semID, &WaitProtectSheepMealFlag, 1);
		*sheepMealFlagp = *sheepMealFlagp + 1;
		printf("SSSSSSS %8lu SSSSSSS   signal sheep meal flag %d\n", localThreadID, *sheepMealFlagp);
		semopChecked(semID, &SignalProtectSheepMealFlag, 1);

		semopChecked(semID, &WaitProtectCowMealFlag, 1);
		if( *cowMealFlagP >= 1 ) {
			semopChecked(semID, &SignalDragonSleeping, 1);
			printf("SSSSSSS %8lu SSSSSSS   last sheep  wakes the dragon \n", localThreadID);
		}
		semopChecked(semID, &SignalProtectCowMealFlag, 1);
	}
	else
	{
		semopChecked(semID, &SignalProtectSheepInGroup, 1);
	}

	semopChecked(semID, &WaitSheepWaiting, 1);

	// Terminate check
	semopChecked(semID, &WaitProtectTerminate, 1);
	if( *terminateFlagp == 1 ) {
		printf("SSSSSSS %8lu SSSSSSS   A sheep has been woken up to be eaten after we've been told to terminate\n", localThreadID);
		semopChecked(semID, &SignalProtectTerminate, 1);
		kill(localThreadID, SIGKILL);
		return;
	} else {
		semopChecked(semID, &SignalProtectTerminate, 1);
		printf("SSSSSSS %8lu SSSSSSS   A sheep has been woken up to be eaten\n", localThreadID);
	}

	/* have all the sheeps in group been eaten? */
	/* if so wake up the dragon */
	semopChecked(semID, &WaitProtectSheepEaten, 1);
	semopChecked(semID, &SignalSheepEaten, 1);
	*sheepEatenCounterp = *sheepEatenCounterp + 1;
	if( ( *sheepEatenCounterp >= SHEEP_IN_GROUP )) {
		*sheepEatenCounterp = *sheepEatenCounterp - SHEEP_IN_GROUP;
		for (k=0; k<SHEEP_IN_GROUP; k++){
			semopChecked(semID, &WaitSheepEaten, 1);
		}
		printf("SSSSSSS %8lu SSSSSSS   The last sheep has been eaten\n", localThreadID);
		semopChecked(semID, &SignalProtectSheepEaten, 1);
		semopChecked(semID, &SignalDragonEating, 1);
	}
	else
	{
		semopChecked(semID, &SignalProtectSheepEaten, 1);
		printf("SSSSSSS %8lu SSSSSSS   A sheep is waiting to be eaten\n", localThreadID);
	}

	semopChecked(semID, &WaitSheepDead, 1);

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
	semopChecked(semID, &WaitProtectCowsInGroup, 1);
	semopChecked(semID, &SignalCowsInGroup, 1);
	*cowCounterp = *cowCounterp + 1;
	printf("CCCCCCC %8lu CCCCCCC   %d  cow has been enchanted \n", localThreadID, *cowCounterp );
	if( ( *cowCounterp  >= COWS_IN_GROUP )) {
		*cowCounterp = *cowCounterp - COWS_IN_GROUP;
		semopChecked(semID, &SignalProtectCowsInGroup, 1);
		for (k=0; k<COWS_IN_GROUP; k++){
			semopChecked(semID, &WaitCowsInGroup, 1);
		}
		printf("CCCCCCC %8lu CCCCCCC   The last cow is waiting\n", localThreadID);
		semopChecked(semID, &WaitProtectCowMealFlag, 1);
		*cowMealFlagP = *cowMealFlagP + 1;
		printf("CCCCCCC %8lu CCCCCCC   signal cow meal flag %d\n", localThreadID, *cowMealFlagP);
		semopChecked(semID, &SignalProtectCowMealFlag, 1);

		semopChecked(semID, &WaitProtectSheepMealFlag, 1);
		if( *sheepMealFlagp >= 1 ) {
			semopChecked(semID, &SignalDragonSleeping, 1);
			printf("CCCCCCC %8lu CCCCCCC   last cow  wakes the dragon \n", localThreadID);
		}	
		semopChecked(semID, &SignalProtectSheepMealFlag, 1);
	}
	else
	{
		semopChecked(semID, &SignalProtectCowsInGroup, 1);
	}

	semopChecked(semID, &WaitCowsWaiting, 1);

	// Terminate check
	semopChecked(semID, &WaitProtectTerminate, 1);
	if( *terminateFlagp == 1 ) {
		printf("CCCCCCC %8lu CCCCCCC   A cow has been woken up to be eaten after we've been told to terminate\n", localThreadID);
		semopChecked(semID, &SignalProtectTerminate, 1);
		kill(localThreadID, SIGKILL);
		return;
	} else {
		semopChecked(semID, &SignalProtectTerminate, 1);
		printf("CCCCCCC %8lu CCCCCCC   A cow has been woken up to be eaten\n", localThreadID);
	}

	/* have all the cows in group been eaten? */
	/* if so wake up the dragon */
	semopChecked(semID, &WaitProtectCowsEaten, 1);
	semopChecked(semID, &SignalCowsEaten, 1);
	*cowsEatenCounterp = *cowsEatenCounterp + 1;
	if( ( *cowsEatenCounterp >= COWS_IN_GROUP )) {
		*cowsEatenCounterp = *cowsEatenCounterp - COWS_IN_GROUP;
		for (k=0; k<COWS_IN_GROUP; k++){
			semopChecked(semID, &WaitCowsEaten, 1);
		}
		printf("CCCCCCC %8lu CCCCCCC   The last cow has been eaten\n", localThreadID);
		semopChecked(semID, &SignalProtectCowsEaten, 1);
		semopChecked(semID, &SignalDragonEating, 1);
	}
	else
	{
		semopChecked(semID, &SignalProtectCowsEaten, 1);
		printf("CCCCCCC %8lu CCCCCCC   A cow is waiting to be eaten\n", localThreadID);
	}
	semopChecked(semID, &WaitCowsDead, 1);

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
	semopChecked(semID, &WaitProtectTerminate, 1);
	if( *terminateFlagp == 1 ) {
		printf("TTTTTTT %8lu TTTTTTT   thief has found the magical path after we've been told to terminate\n", localThreadID);
		semopChecked(semID, &SignalProtectTerminate, 1);
		return NULL;
	} else {
		printf("TTTTTTT %8lu TTTTTTT   thief has found the magical path in %f ms\n", localThreadID, startTimeN);
		semopChecked(semID, &SignalProtectTerminate, 1);
	}

	semopChecked(semID, &WaitProtectThiefCount, 1);
	*thiefCounterp = *thiefCounterp + 1;
	semopChecked(semID, &SignalProtectThiefCount, 1);
	printf("TTTTTTT %8lu TTTTTTT   thief is under smaug's spell and is waiting to be interacted with\n", localThreadID);
	printf("TTTTTTT %8lu TTTTTTT   thief wakes smaug\n", localThreadID);
	semopChecked(semID, &SignalDragonSleeping, 1);
	semopChecked(semID, &WaitThievesWaiting, 1);
	printf("TTTTTTT %8lu TTTTTTT   thief enters smaug's cave\n", localThreadID);
	printf("TTTTTTT %8lu TTTTTTT   thief plays with smaug\n", localThreadID);
	semopChecked(semID, &WaitThiefFinish, 1);
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
	semopChecked(semID, &WaitProtectTerminate, 1);
	if( *terminateFlagp == 1 ) {
		printf("HHHHHHH %8lu HHHHHHH   hunter has found the magical path after we've been told to terminate\n", localThreadID);
		semopChecked(semID, &SignalProtectTerminate, 1);
		return NULL;
	} else {
		printf("HHHHHHH %8lu HHHHHHH   hunter has found the magical path in %f ms\n", localThreadID, startTimeN);
		semopChecked(semID, &SignalProtectTerminate, 1);
	}

	semopChecked(semID, &WaitProtectHunterCount, 1);
	*hunterCounterp = *hunterCounterp + 1;
	semopChecked(semID, &SignalProtectHunterCount, 1);
	printf("HHHHHHH %8lu HHHHHHH   hunter is under smaug's spell and is waiting to be interacted with\n", localThreadID);
	printf("HHHHHHH %8lu HHHHHHH   hunter wakes smaug\n", localThreadID);
	semopChecked(semID, &SignalDragonSleeping, 1);
	semopChecked(semID, &WaitHuntersWaiting, 1);
	printf("HHHHHHH %8lu HHHHHHH   hunter enters smaug's cave\n", localThreadID);
	printf("HHHHHHH %8lu HHHHHHH   hunter fights smaug\n", localThreadID);
	semopChecked(semID, &WaitHunterFinish, 1);
	printf("TTTTTTT %8lu TTTTTTT   hunter leaves cave and goes home\n", localThreadID);

	return NULL;
}


void terminateSimulation() {
	pid_t localpgid;
	pid_t localpid;
	int w = 0;
	int status;

	localpid = getpid();
	printf("RELEASESEMAPHORES   Terminating Simulation from process %8d\n", localpid);
	if(sheepProcessGID != (int)localpgid ){
		if(killpg(sheepProcessGID, SIGKILL) == -1 && errno == EPERM) {
			printf("XXTERMINATETERMINATE   SHEEPS NOT KILLED\n");
		}
		printf("XXTERMINATETERMINATE   killed sheeps \n");
	}
	if(cowProcessGID != (int)localpgid ){
		if(killpg(cowProcessGID, SIGKILL) == -1 && errno == EPERM) {
			printf("XXTERMINATETERMINATE   COWS NOT KILLED\n");
		}
		printf("XXTERMINATETERMINATE   killed cows \n");
	}
	if(hunterProcessGID != (int)localpgid ){
		if(killpg(hunterProcessGID, SIGKILL) == -1 && errno == EPERM) {
			printf("XXTERMINATETERMINATE   HUNTERS NOT KILLED\n");
		}
		printf("XXTERMINATETERMINATE   killed hunters \n");
	}

	//printf("smaugProcessID: %d  localpgid: %d\n", smaugProcessID, localpgid);

	if(smaugProcessID != (int)localpid ) {
		kill(smaugProcessID, SIGKILL);
		printf("XXTERMINATETERMINATE   killed smaug\n");
	}
	while( (w = waitpid( -1, &status, WNOHANG)) > 1){
			printf("                           REAPED process in terminate %d\n", w);
	}
	releaseSemandMem();
	printf("GOODBYE from terminate\n");
}

void releaseSemandMem() 
{
	pid_t localpid;
	int w = 0;
	int status;

	localpid = getpid();

	//should check return values for clean termination
	semctl(semID, 0, IPC_RMID, seminfo);


	// wait for the semaphores 
	usleep(4000);
	// arg1 is -1 to wait for all child processes
	while( (w = waitpid( -1, &status, WNOHANG)) > 1){
		printf("                           REAPED process in terminate %d\n", w);
	}
	printf("\n");
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

void semctlChecked(int semaphoreID, int semNum, int flag, union semun seminfo) { 
	/* wrapper that checks if the semaphore control request has terminated */
	/* successfully. If it has not the entire simulation is terminated */

	if (semctl(semaphoreID, semNum, flag,  seminfo) == -1 ) {
		if(errno != EIDRM) {
			printf("semaphore control failed: simulation terminating\n");
			printf("errno %8d \n",errno );
			*terminateFlagp = 1;
			releaseSemandMem();
			exit(2);
		}
		else {
			exit(3);
		}
	}
}

void semopChecked(int semaphoreID, struct sembuf *operation, unsigned something) 
{
	/* wrapper that checks if the semaphore operation request has terminated */
	/* successfully. If it has not the entire simulation is terminated */
	if (semop(semaphoreID, operation, something) == -1 ) {
		if(errno != EIDRM) {
			printf("semaphore operation failed: simulation terminating\n");
			*terminateFlagp = 1;
			releaseSemandMem();
			exit(2);
		}
		else {
			exit(3);
		}
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

void *mt(void *varp) {
	int *var = (int *)varp;
	while(++(*var) < 10);
	return NULL;
}
int main() {
	pthread_t mythread;
	int var = 0;
	if(pthread_create(&mythread, NULL, mt, &var)) {
		printf("error creating thread!\n");
		return 1;
	}
	if(pthread_join(mythread, NULL)) {
		printf("error joining thread!\n");
		return 1;
	}
	printf("%d\n", var);


	initialize();

	printf("1s (1 second) is 1000000us (1e6 microseconds)\n");
	const int maximumSheepInterval = getInputFor("maximumSheepInterval (us)");
	const int maximumCowInterval = getInputFor("maximumCowInterval (us)");
	const int maximumHunterInterval = getInputFor("maximumHunterInterval (us)");
	const int maximumThiefInterval = getInputFor("maximumThiefInterval (us)");
	const int smaugWinProb = getInputFor("smaugWinProb (0 to 100)");

	double sheepTimer = 0;
	double cowTimer = 0;
	double hunterTimer = 0;
	double thiefTimer = 0;

	parentProcessID = getpid();
	// we do not know smaugpid yet
	smaugProcessID = -1; 
	sheepProcessGID = parentProcessID - 1;
	cowProcessGID = parentProcessID - 2;
	hunterProcessGID = parentProcessID - 3;
	thiefProcessGID = parentProcessID - 4;

	pthread_t smaugThread;
	if(pthread_create(&smaugThread, NULL, smaug, &smaugWinProb)) {
		printf("error creating thread!\n");
		return 1;
	}
	pthread_detach(smaugThread);

	/*pid_t childPID = fork();

	if(childPID < 0) {
		printf("FORK FAILED\n");
		return 1;
	} else if(childPID == 0) {
		smaug(smaugWinProb);
		return 0;
	} 

	// smaugpid is now known to callee from the above fork; assign it now
	smaugProcessID = childPID; */
		
	gettimeofday(&startTime, NULL);
	int zombieTick = 0;
	while(*terminateFlagp == 0) {
		zombieTick++;
		double simDuration = timeChange(startTime);

		if(sheepTimer - simDuration <= 0) {
			sheepTimer = simDuration + (rand() % maximumSheepInterval) / 1000.0;
			printf("SHEEP CREATED! next sheep at: %f\n", sheepTimer);
			float sleepTime = (rand() % maximumSheepInterval) / 1000.0;
			pthread_t sheepThread;
			pthread_create(&sheepThread, NULL, sheep, &sleepTime);
			//printf("sheepthread: %d\n", (int)sheepThread);
			pthread_detach(sheepThread);
		}

		if(cowTimer - simDuration <= 0) {
			cowTimer = simDuration + (rand() % maximumCowInterval) / 1000.0;
			printf("COW CREATED! next cow at: %f\n", cowTimer);
			float cowTime = (rand() % maximumCowInterval) / 1000.0;
			pthread_t cowThread;
			pthread_create(&cowThread, NULL, cow, &cowTime);
			pthread_detach(cowThread);
		}

		if(thiefTimer - simDuration <= 0) {
			thiefTimer = simDuration + (rand() % maximumThiefInterval) / 1000.0;
			printf("THIEF CREATED! next thief at: %f\n", thiefTimer);
			float thiefTime = (rand() % maximumThiefInterval) / 1000.0;
			pthread_t thiefThread;
			pthread_create(&thiefThread, NULL, thief, &thiefTime);
			pthread_detach(thiefThread);
		}

		if(hunterTimer - simDuration <= 0) {
			hunterTimer = simDuration + (rand() % maximumHunterInterval) / 1000.0;
			printf("HUNTER CREATED! next hunter at: %f\n", hunterTimer);
			float hunterTime = (rand() % maximumHunterInterval) / 1000.0;
			pthread_t hunterThread;
			pthread_create(&hunterThread, NULL, hunter, &hunterTime);
			pthread_detach(hunterThread);
		} 
/*
		// Purge all zombies every 10 iteratinos
		if(zombieTick % 10 == 0) {
			zombieTick -= 10;
			// arg1 is -1 to wait for all child processes
			int w = 0; int status = 0;
			while( (w = waitpid( -1, &status, WNOHANG)) > 1){
				printf("                           REAPED zombie process %d from main loop\n", w);
			}
		} */
	}
	
	//	printf("testing values: %d\n", maximumsheepinterval);

	terminateSimulation();
	return 0;
}

int getInputFor(char *prompt) {
	printf("Enter the value for %s: ", prompt);
	int input = 0;
	scanf("%d", &input);
	return input;
}

