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
#define SEM_COWSWAITING 7
#define SEM_PCOWSEATEN 11
#define SEM_COWSEATEN 12
#define SEM_COWSDEAD 16
#define SEM_PTERMINATE 17
#define SEM_DRAGONEATING 19
#define SEM_DRAGONFIGHTING 20
#define SEM_DRAGONSLEEPING 21
#define SEM_PMEALWAITINGFLAG 23


/* System constants used to control simulation termination */
#define MAX_COWS_EATEN 12
#define MAX_COWS_CREATED 80
#define MAX_TREASURE_IN_HOARD 120
#define INITIAL_TREASURE_IN_HOARD 100


/* System constants to specify size of groups of cows*/
#define COWS_IN_GROUP 4

/* CREATING YOUR SEMAPHORES */
int semID; 

union semun
{
	int val;
	struct semid_ds *buf;
	ushort *array;
} seminfo;

struct timeval startTime;


/*  Pointers and ids for shared memory segments */
int *terminateFlagp = NULL;
int *cowCounterp = NULL;
int *cowsEatenCounterp = NULL;
int *mealWaitingFlagp = NULL;
int terminateFlag = 0;
int cowCounter = 0;
int cowsEatenCounter = 0;
int mealWaitingFlag = 0;

/* Group IDs for managing/removing processes */
int smaugProcessID = -1;
int cowProcessGID = -1;
int parentProcessGID = -1;


/* Define the semaphore operations for each semaphore */
/* Arguments of each definition are: */
/* Name of semaphore on which the operation is done */
/* Increment (amount added to the semaphore when operation executes*/
/* Flag values (block when semaphore <0, enable undo ...)*/

/*Number in group semaphores*/
struct sembuf WaitCowsInGroup={SEM_COWSINGROUP, -1, 0};
struct sembuf SignalCowsInGroup={SEM_COWSINGROUP, 1, 0};

/*Number in group mutexes*/
struct sembuf WaitProtectCowsInGroup={SEM_PCOWSINGROUP, -1, 0};
struct sembuf WaitProtectMealWaitingFlag={SEM_PMEALWAITINGFLAG, -1, 0};
struct sembuf SignalProtectCowsInGroup={SEM_PCOWSINGROUP, 1, 0};
struct sembuf SignalProtectMealWaitingFlag={SEM_PMEALWAITINGFLAG, 1, 0};

/*Number waiting sempahores*/
struct sembuf WaitCowsWaiting={SEM_COWSWAITING, -1, 0};
struct sembuf SignalCowsWaiting={SEM_COWSWAITING, 1, 0};

/*Number eaten or fought semaphores*/
struct sembuf WaitCowsEaten={SEM_COWSEATEN, -1, 0};
struct sembuf SignalCowsEaten={SEM_COWSEATEN, 1, 0};

/*Number eaten or fought mutexes*/
struct sembuf WaitProtectCowsEaten={SEM_PCOWSEATEN, -1, 0};
struct sembuf SignalProtectCowsEaten={SEM_PCOWSEATEN, 1, 0};

/*Number Dead semaphores*/
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
void smaug();
void cow(int startTimeN);
void terminateSimulation();
void releaseSemandMem();
void semopChecked(int semaphoreID, struct sembuf *operation, unsigned something); 
void semctlChecked(int semaphoreID, int semNum, int flag, union semun seminfo); 
	


void smaug()
{
	int k;
	int temp;
	int localpid;
	double elapsedTime;

	/* local counters used only for smaug routine */
	int cowsEatenTotal = 0;


	/* Initialize random number generator*/
	/* Random numbers are used to determine the time between successive beasts */
	smaugProcessID = getpid();
	printf("SMAUGSMAUGSMAUGSMAUGSMAU   PID is %d \n", smaugProcessID );
	localpid = smaugProcessID;
	printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has gone to sleep\n" );
	semopChecked(semID, &WaitDragonSleeping, 1);
	printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has woken up \n" );
	while (TRUE) {		
		semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
		while( *mealWaitingFlagp >= 1  ) {
			*mealWaitingFlagp = *mealWaitingFlagp - 1;
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   signal meal flag %d\n", *mealWaitingFlagp);
			semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is eating a meal\n");
			for( k = 0; k < COWS_IN_GROUP; k++ ) {
				semopChecked(semID, &SignalCowsWaiting, 1);
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   A cow is ready to eat\n");
			}

			/*Smaug waits to eat*/
			semopChecked(semID, &WaitDragonEating, 1);
			for( k = 0; k < COWS_IN_GROUP; k++ ) {
				semopChecked(semID, &SignalCowsDead, 1);
				cowsEatenTotal++;
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug finished eating a cow\n");
			}
			printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has finished a meal\n");
			if(cowsEatenTotal >= MAX_COWS_EATEN ) {
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug has eaten the allowed number of cows\n");
				*terminateFlagp= 1;
				break; 
			}

			/* Smaug checke to see if another snack is waiting */
			semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
			if( *mealWaitingFlagp > 0  ) {
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug eats again\n", localpid);
				continue;
			}
			else {
				semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug sleeps again\n", localpid);
				semopChecked(semID, &WaitDragonSleeping, 1);
				printf("SMAUGSMAUGSMAUGSMAUGSMAU   Smaug is awake again\n", localpid);
				break;
			}
		}
	}
}


void initialize()
{
	/* Init semaphores */
	semID=semget(IPC_PRIVATE, 25, 0666 | IPC_CREAT);


	/* Init to zero, no elements are produced yet */
	seminfo.val=0;
	semctlChecked(semID, SEM_COWSINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSWAITING, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSEATEN, SETVAL, seminfo);
	semctlChecked(semID, SEM_COWSDEAD, SETVAL, seminfo);
	semctlChecked(semID, SEM_DRAGONFIGHTING, SETVAL, seminfo);
	semctlChecked(semID, SEM_DRAGONSLEEPING, SETVAL, seminfo);
	semctlChecked(semID, SEM_DRAGONEATING, SETVAL, seminfo);
	printf("!!INIT!!INIT!!INIT!!  semaphores initiialized\n");
	
	/* Init Mutex to one */
	seminfo.val=1;
	semctlChecked(semID, SEM_PCOWSINGROUP, SETVAL, seminfo);
	semctlChecked(semID, SEM_PMEALWAITINGFLAG, SETVAL, seminfo);
	semctlChecked(semID, SEM_PCOWSEATEN, SETVAL, seminfo);
	semctlChecked(semID, SEM_PTERMINATE, SETVAL, seminfo);
	printf("!!INIT!!INIT!!INIT!!  mutexes initiialized\n");


	/* Now we create and attach  the segments of shared memory*/
        if ((terminateFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
                printf("!!INIT!!INIT!!INIT!!  shm not created for terminateFlag\n");
                 exit(1);
        }
        else {
                printf("!!INIT!!INIT!!INIT!!  shm created for terminateFlag\n");
        }
	if ((cowCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for cowCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for cowCounter\n");
	}
	if ((mealWaitingFlag = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for mealWaitingFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for mealWaitingFlag\n");
	}
	if ((cowsEatenCounter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0) {
		printf("!!INIT!!INIT!!INIT!!  shm not created for cowsEatenCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm created for cowsEatenCounter\n");
	}


	/* Now we attach the segment to our data space.  */
        if ((terminateFlagp = shmat(terminateFlag, NULL, 0)) == (int *) -1) {
                printf("!!INIT!!INIT!!INIT!!  shm not attached for terminateFlag\n");
                exit(1);
        }
        else {
                 printf("!!INIT!!INIT!!INIT!!  shm attached for terminateFlag\n");
        }

	if ((cowCounterp = shmat(cowCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for cowCounter\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for cowCounter\n");
	}
	if ((mealWaitingFlagp = shmat(mealWaitingFlag, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for mealWaitingFlag\n");
		exit(1);
	}
	else {
		printf("!!INIT!!INIT!!INIT!!  shm attached for mealWaitingFlag\n");
	}
	if ((cowsEatenCounterp = shmat(cowsEatenCounter, NULL, 0)) == (int *) -1) {
		printf("!!INIT!!INIT!!INIT!!  shm not attached for cowsEatenCounter\n");
		exit(1);
		printf("!!INIT!!INIT!!INIT!!  shm attached for cowsEatenCounter\n");
	}
	printf("!!INIT!!INIT!!INIT!!   initialize end\n");
}



void cow(int startTimeN)
{
	int localpid;
	int retval;
	int k;
	localpid = getpid();

	/* graze */
	printf("CCCCCCC %8d CCCCCCC   A cow is born\n", localpid);
	if( startTimeN > 0) {
		if( usleep( startTimeN) == -1){
			/* exit when usleep interrupted by kill signal */
			if(errno==EINTR)exit(4);
		}	
	}
	printf("CCCCCCC %8d CCCCCCC   cow grazes for %f ms\n", localpid, startTimeN/1000.0);


	/* does this beast complete a group of BEASTS_IN_GROUP ? */
	/* if so wake up the dragon */
	semopChecked(semID, &WaitProtectCowsInGroup, 1);
	semopChecked(semID, &SignalCowsInGroup, 1);
	*cowCounterp = *cowCounterp + 1;
	printf("CCCCCCC %8d CCCCCCC   %d  cows have been enchanted \n", localpid, *cowCounterp );
	if( ( *cowCounterp  >= COWS_IN_GROUP )) {
		*cowCounterp = *cowCounterp - COWS_IN_GROUP;
		for (k=0; k<COWS_IN_GROUP; k++){
			semopChecked(semID, &WaitCowsInGroup, 1);
		}
		printf("CCCCCCC %8d CCCCCCC   The last cow is waiting\n", localpid);
		semopChecked(semID, &WaitProtectMealWaitingFlag, 1);
		*mealWaitingFlagp = *mealWaitingFlagp + 1;
		printf("CCCCCCC %8d CCCCCCC   signal meal flag %d\n", localpid, *mealWaitingFlagp);
		semopChecked(semID, &SignalProtectMealWaitingFlag, 1);
		semopChecked(semID, &SignalDragonSleeping, 1);
		printf("CCCCCCC %8d CCCCCCC   last cow  wakes the dragon \n", localpid);
	}
	else
	{
		semopChecked(semID, &SignalProtectCowsInGroup, 1);
	}

	semopChecked(semID, &WaitCowsWaiting, 1);

	/* have all the beasts in group been eaten? */
	/* if so wake up the dragon */
	semopChecked(semID, &WaitProtectCowsEaten, 1);
	semopChecked(semID, &SignalCowsEaten, 1);
	*cowsEatenCounterp = *cowsEatenCounterp + 1;
	if( ( *cowsEatenCounterp >= COWS_IN_GROUP )) {
		*cowsEatenCounterp = *cowsEatenCounterp - COWS_IN_GROUP;
		for (k=0; k<COWS_IN_GROUP; k++){
       		        semopChecked(semID, &WaitCowsEaten, 1);
		}
		printf("CCCCCCC %8d CCCCCCC   The last cow has been eaten\n", localpid);
		semopChecked(semID, &SignalProtectCowsEaten, 1);
		semopChecked(semID, &SignalDragonEating, 1);
	}
	else
	{
		semopChecked(semID, &SignalProtectCowsEaten, 1);
		printf("CCCCCCC %8d CCCCCCC   A cow is waiting to be eaten\n", localpid);
	}
	semopChecked(semID, &WaitCowsDead, 1);

	printf("CCCCCCC %8d CCCCCCC   cow  dies\n", localpid);
}



void terminateSimulation() {
	pid_t localpgid;
	pid_t localpid;
	int w = 0;
	int status;

	localpid = getpid();
	printf("RELEASESEMAPHORES   Terminating Simulation from process %8d\n", localpgid);
	if(cowProcessGID != (int)localpgid ){
		if(killpg(cowProcessGID, SIGKILL) == -1 && errno == EPERM) {
			printf("XXTERMINATETERMINATE   COWS NOT KILLED\n");
		}
		printf("XXTERMINATETERMINATE   killed cows \n");
	}
	if(smaugProcessID != (int)localpgid ) {
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
	usleep(2000);
	while( (w = waitpid( -1, &status, WNOHANG)) > 1){
			printf("                           REAPED process in terminate %d\n", w);
	}
	printf("\n");
        if(shmdt(terminateFlagp)==-1) {
                printf("RELEASERELEASERELEAS   terminateFlag share memory detach failed\n");
        }
        else{
                printf("RELEASERELEASERELEAS   terminateFlag share memory detached\n");
        }
        if( shmctl(terminateFlag, IPC_RMID, NULL ))
        {
                printf("RELEASERELEASERELEAS   share memory delete failed %d\n",*terminateFlagp );
        }
        else{
                printf("RELEASERELEASERELEAS   share memory deleted\n");
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
	if( shmdt(mealWaitingFlagp)==-1)
	{
		printf("RELEASERELEASERELEAS   mealWaitingFlagp memory detach failed\n");
	}
	else{
		printf("RELEASERELEASERELEAS   mealWaitingFlagp memory detached\n");
	}
	if( shmctl(mealWaitingFlag, IPC_RMID, NULL ))
	{
		printf("RELEASERELEASERELEAS   mealWaitingFlag share memory delete failed \n");
	}
	else{
		printf("RELEASERELEASERELEAS   mealWaitingFlag share memory deleted\n");
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


struct CallbackInterval {
	int remainingTime;
	int interval;
};

typedef struct CallbackInterval Interval;

void processInterval(Interval i) {
	i.remainingTime -= 1;
	if( i.remainingTime< 0 )
		i.remainingTime = 0;
}

int isIntervalReady(Interval i) {
	if( i.remainingTime == 0 )
		return 1;
	else
		return 0;
}

int main() {
	initialize();

	int done = 0;

	Interval maximumSheepInterval;
	int maximumCowInterval;
	int maximumHunterInterval;
	int maximumThiefInterval;
	int winProb;

	maximumSheepInterval.interval = getInputFor("maximumSheepInterval");

	maximumCowInterval = getInputFor("maximumCowInterval");
	maximumHunterInterval = getInputFor("maximumHunterInterval");
	maximumThiefInterval = getInputFor("maximumThiefInterval");
	winProb = getInputFor("winProb");

	pid_t childPID = fork();

	if(childPID >= 0) {
		if(childPID == 0) {
			smaug();
		} else {
				
			int c = 0;
			while(done != 1) {
				if(c < 10)
					printf("tick: %d\n", c++);

				processInterval(maximumSheepInterval);

				if(isIntervalReady(maximumSheepInterval) == 1) {
					printf("o ya\n");
				}


				usleep(100);
			}
		
			//	printf("testing values: %d\n", maximumsheepinterval);
			//	printf("testing values: %d\n", maximumCowInterval);
			//	printf("testing values: %d\n", winProb);

		}
	} else {
		printf("FORK FAILED\n");
		return 1;
	}

	return 0;
}

int getInputFor(char *prompt) {
	printf("Enter the value for %s: ", prompt);
	int input = 0;
	scanf("%d", &input);
	return input;
}
