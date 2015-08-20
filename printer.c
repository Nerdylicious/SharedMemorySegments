/**
 * printer.c
 *
 * PURPOSE:		A print client and print server program using shared 
 *                  memory segments.
 *
 * NOTE:	Sometimes the output for a client inserting and a server finishing
 *        will get "mushed" together, this sometimes happens when they get
 *        outputted at the same time.
 *        I also had to give unique names to the semaphores, like myownemptyslot
 *        because there have been times when if I used a more generic name I would
 *        get permission denied if I forgot to unlink and close the semaphore, although
 *        I have fixed this issue and it is no longer a problem.
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h> 	
#include <time.h>
#include <sys/stat.h>

#define SHMSZ     200
#define BUFFER_SIZE 3

typedef struct PR{
	long clientID;
	char fileName[64];	//ptr unusable in shared memory
	int fileSize;
} PrintRequest;

//PrintRequest *BoundedBuffer[4];
PrintRequest *BoundedBuffer;
int *length_buffer;
int *front;
int *back;
	

void insertIntoBuffer(PrintRequest request){

	sem_t *mutex = sem_open("/myownmutex", 0, 0644, 0);
	if(mutex == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	sem_t *empty_slot = sem_open("/myownemptyslot", 0, 0644, 0); 
	if(empty_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	sem_t *full_slot = sem_open("/myownfullslot", 0, 0644, 0);
	if(full_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}

	time_t current_time;
	
	//wait for an empty slot
	sem_wait(empty_slot);
	sem_wait(mutex);

	//put item into buffer
	(*length_buffer) ++;
	*back = ((*front) + (*length_buffer) - 1) % BUFFER_SIZE;
	
	BoundedBuffer[*back].clientID = request.clientID;
	sprintf(BoundedBuffer[*back].fileName, "%s", request.fileName);
	BoundedBuffer[*back].fileSize = request.fileSize;
	
	time(&current_time);
	printf("\n\nClient <pid=%d> insert\nTime: %sClient ID: %ld\nFile: %s\nFile Size: %d", getpid(), ctime(&current_time), request.clientID, request.fileName, request.fileSize);
	fflush(stdout);
	
	sem_post(mutex);
	//signal there is a full slot
	sem_post(full_slot);

}

PrintRequest removeFromBuffer(){

	sem_t *mutex = sem_open("/myownmutex", 0, 0644, 0);
	if(mutex == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	sem_t *empty_slot = sem_open("/myownemptyslot", 0, 0644, 0); 
	if(empty_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	sem_t *full_slot = sem_open("/myownfullslot", 0, 0644, 0);
	if(full_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}

	//wait for a full slot
	sem_wait(full_slot);
	sem_wait(mutex);

	PrintRequest request;

	request.clientID = BoundedBuffer[*front].clientID;
	sprintf(request.fileName, "%s", BoundedBuffer[*front].fileName);
	request.fileSize = BoundedBuffer[*front].fileSize;

	(*length_buffer) --;
	*front = (*front + 1) % BUFFER_SIZE;
	
	sem_post(mutex);
	//signal there is an empty slot
	sem_post(empty_slot);
	
	return request;
}

PrintRequest create_print_request(int i){

	PrintRequest request;
	char *pid = malloc(sizeof(char)*50);
	char *iteration = malloc(sizeof(int));
	char *file = "FILE";
	char *delimiter = "_";
	int randnum;
	
	request.clientID = getpid();
	
	//construct the filename
	sprintf(pid, "%d", getpid());
	sprintf(iteration, "%d", i + 1);
	sprintf(request.fileName, "%s%s%s%s%s", file, delimiter, pid, delimiter, iteration);	
	
	randnum = rand() % 39501 + 500;	//ranges from 500 to 40000
	request.fileSize = randnum;
		
	return request;
}

void PrintClient(){

	PrintRequest request;
	int sleep_time;

	int i;
	for(i = 0; i < 6; i ++){
		
		request = create_print_request(i);
		insertIntoBuffer(request);
		
		sleep_time = rand() % 3 + 1;
		sleep(sleep_time);
	}

}

void PrintServer(){
	
	PrintRequest request;
	time_t current_time;
	int sleep_time;
	
	while(1){
		request = removeFromBuffer();
		
		time(&current_time);
		printf("\n\nServer <pid=%d> removed\nTime: %sClient ID: %ld\nFile: %s\nFile Size: %d\nStarting print job", getpid(), ctime(&current_time), request.clientID, request.fileName, request.fileSize);
		fflush(stdout);
		
		sleep_time = request.fileSize / 7000; //assume filesize is in number of chars
		sleep(sleep_time);	
		
		time(&current_time);
		printf("\n\nServer <pid=%d> finished\nTime: %sClient ID: %ld\nFile: %s\nFile Size: %d\nPrint job complete", getpid(), ctime(&current_time), request.clientID, request.fileName, request.fileSize);
		fflush(stdout);
	}

}

int main(int argc, char *argv[]) {

	sem_t *mutex;
	sem_t *empty_slot;
	sem_t *full_slot;

	
	int pids[10];
	int counter = 0;
	int shmid;
	void *shared_mem = (void *)0;
	key_t key;
	key = 7639184;
	srand(time(NULL));
	
	mutex = sem_open("/myownmutex", O_CREAT, 0644,1);
	if(mutex == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	empty_slot = sem_open("/myownemptyslot", O_CREAT, 0644, BUFFER_SIZE); 
	if(empty_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}
	full_slot = sem_open("/myownfullslot", O_CREAT, 0644, 0);
	if(full_slot == SEM_FAILED){
		perror("Error: Cannot open semaphore");
		sem_close(mutex);
		exit(-1);
    	}

	//create the segment
	if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
	   perror("shmget failed");
	   exit(1);
	}
	
	//attach the segment
	if ((shared_mem = shmat(shmid, NULL, 0)) == (void *) -1) {
		perror("shmat failed");
		exit(1);
	}

	length_buffer = (int *) shared_mem;
	front = (int *) shared_mem + 1;
	back = (int *) shared_mem + 2;
	BoundedBuffer = (PrintRequest *) shared_mem + 3;
	
	*length_buffer = 0;
	*front = 0;
	*back = 0;

	pids[counter] = fork();
	if(pids[counter] != 0){
		counter ++;
	}
	else{
		PrintClient();
		exit(0);

	}
	pids[counter] = fork();
	if(pids[counter] != 0){
		counter ++;
	}
	else{
		PrintServer();
		exit(0);
	
	}

	int i;
	for(i = 0; i < counter; i ++){
		//wait until all created child processes are done
		int status;
		while(waitpid(pids[i], &status, 0));
		
			//do unlinking when first child is done
			sem_unlink("/myownmutex");
			sem_unlink("/myownemptyslot");
			sem_unlink("/myownfullslot");

			sem_close(mutex);
			sem_close(empty_slot);
			sem_close(full_slot);
		
	}

	return 0;
}













