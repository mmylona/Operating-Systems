#include <stdio.h> 
#include <stdlib.h>
#include <time.h>
#include <string.h> 

#include <semaphore.h> 
#include <sys/wait.h>
#include <sys/ipc.h>  
#include <sys/shm.h> 
#include <sys/types.h> 
#include <unistd.h> 

#define BUFFER_SIZE 50 
#define MAX_LINE_LENGTH 150

struct childRequest { 
    int child_id; 
    int segment; 
    int line; 
}; 

struct sharedRequest { 
    sem_t request_mutex; 
    sem_t request_empty; 
    sem_t request_full; 
    sem_t print_mutex; 
    int in; 
    int out; 
    struct childRequest buffer[BUFFER_SIZE]; 
}; 

struct sharedResponse { 
    sem_t response_parent; 
    sem_t response_child;  
    char response_line[MAX_LINE_LENGTH+1]; 
};

//global variables
struct sharedRequest *sh_request_ptr = NULL; 
struct sharedResponse **sh_response_ptr = NULL;  
int *shm_response_id; 
int N; 


void childProccess(int id, int child_requests, int num_segment, int segment_lines, int extra){
	FILE *fp_child;
	struct sharedResponse *response_ptr = NULL; 
    struct childRequest child_request;
	char child_file[30];
	time_t request_time;

	srand(time(NULL)^(getpid()));

	sprintf(child_file, "child_output%d.txt", id);
	fp_child = fopen(child_file, "w"); 

    //to have atached only the shared response of the child
	for (int i=0;i<N;i++)
        if (i != id)
            shmdt(sh_response_ptr[i]);

    response_ptr = sh_response_ptr[id]; 

    child_request.child_id = id;

    // create random request with probability 0.7 to have the same segment
    for(int i=0;i<child_requests;i++){
        double prob =(double)rand() <= RAND_MAX *0.3; 
        if(i == 0 || prob)
            child_request.segment = rand() % num_segment;
        if((child_request.segment == num_segment) && (extra != 0))
            child_request.line = rand() % extra;
        else
            child_request.line = rand() % segment_lines;

        //producer-consumer
        sem_wait(&(sh_request_ptr->request_empty)); 
        sem_wait(&(sh_request_ptr->request_mutex));
        sem_wait(&(sh_request_ptr->print_mutex)); 
        printf("Child %d request %d - Sending line %d of segment %d \n", id, i, child_request.line, child_request.segment); 
        sem_post(&(sh_request_ptr->print_mutex));
        sh_request_ptr->buffer[sh_request_ptr->in] = child_request; 
        sh_request_ptr->in = (sh_request_ptr->in + 1) % BUFFER_SIZE; 
        request_time = time(0); 
        sem_post(&(sh_request_ptr->request_mutex)); 
        sem_post(&(sh_request_ptr->request_full)); 
       

        //child receives 
        sem_wait(&(response_ptr->response_child)); 
        sem_wait(&(sh_request_ptr->print_mutex)); 
        printf("Child %d - Receiving line %d of segment %d :\n", id, child_request.line, child_request.segment); 
        printf(" Request line: %s \n", response_ptr->response_line); 
        sem_post(&(sh_request_ptr->print_mutex)); 
        sem_post(&(response_ptr->response_parent)); 

        //print to child file
        sem_wait(&(sh_request_ptr->print_mutex));
        fprintf(fp_child, "Request time: %ld", request_time);
        fprintf(fp_child, "Response time: %ld\n", time(0));
        fprintf(fp_child, "Request: <%d,%d>\n", child_request.segment, child_request.line);
        fprintf(fp_child, "Request line: %s\n", response_ptr->response_line);
        sem_post(&(sh_request_ptr->print_mutex));

        usleep(20000);
   
    }
    shmdt(response_ptr); 
    fclose(fp_child);

    sem_wait(&(sh_request_ptr->print_mutex)); 
    printf("Child %d received all the requests\n", id); 
    sem_post(&(sh_request_ptr->print_mutex));

    exit(0);
}

int main(int argc, char *argv[]){
    struct childRequest request;
    FILE *fp_input, *fp_parent;
    char **segment;
    char *input_file ;
    int segment_lines;
    int request_per_child;
    int shm_id;
    int file_lines = 0;
    int ch;
    int num_segment;
    int extra;
    int current_segment = -1;

    input_file = argv[1];
    N = atoi(argv[2]);
    segment_lines = atoi(argv[3]);
    request_per_child = atoi(argv[4]);


    //shared memory segment for request
    shm_id = shmget(IPC_PRIVATE, sizeof(struct sharedRequest), 0777 | IPC_CREAT);
    if (shm_id < 0){
        perror("error\n");
        exit(1);
    }
    sh_request_ptr = (struct sharedRequest *)shmat(shm_id, NULL, 0);
    if (sh_request_ptr < 0){
        perror("error\n");
        exit(1);
    }
    sem_init(&(sh_request_ptr->request_mutex), 1, 1); 
    sem_init(&(sh_request_ptr->request_empty), 1, N); 
    sem_init(&(sh_request_ptr->request_full), 1, 0); 
    sem_init(&(sh_request_ptr->print_mutex), 1, 1); 
    sh_request_ptr->in = 0; 
    sh_request_ptr->out = 0; 


    //shared memory segment for response
    shm_response_id = (int *)malloc(N*sizeof(int));
    sh_response_ptr = (struct sharedResponse **)malloc(N * sizeof(struct sharedResponse *));
    for (int i = 0; i < N; i++){ 
        shm_response_id[i] = shmget(IPC_PRIVATE, sizeof(struct sharedResponse), 0777 | IPC_CREAT);
        if (shm_response_id < 0){
            perror("error\n");
            exit(1);
        }
        sh_response_ptr[i] = (struct sharedResponse *)shmat(shm_response_id[i], NULL, 0); 
        if (sh_response_ptr[i] < 0){
            perror("error\n");
            exit(1);
        }
        sem_init(&(sh_response_ptr[i]->response_parent), 1, 1); 
        sem_init(&(sh_response_ptr[i]->response_child), 1, 0);
    }

    // create the segment space
    segment = (char **)malloc(segment_lines * sizeof(char *)); 
    for (int i=0;i<segment_lines;i++)
        segment[i] = (char *)malloc((MAX_LINE_LENGTH+1)*sizeof(char)); 


    //open the input file
    fp_input = fopen(input_file, "r");
    if (fp_input == NULL){
        perror("error\n");
        exit(1);
    }
    
    ch = fgetc(fp_input);
    while(ch != EOF){
        if(ch == '\n')
            file_lines++;
        ch = fgetc(fp_input);
    }
    num_segment = file_lines / segment_lines;
    //find if division does not divide exactly
    extra = file_lines % segment_lines;
    if(extra != 0)
        num_segment++;

    sem_wait(&(sh_request_ptr->print_mutex));
    printf("Number of file lines: %d \n", file_lines); 
    printf("Number of segments in the file: %d \n", num_segment); 
    printf("Number of segment lines: %d \n", segment_lines); 
    printf("\n"); 
    sem_post(&(sh_request_ptr->print_mutex));

    //create processes
    for(int i=0;i<N;i++)
        if(fork() == 0)
            childProccess(i, request_per_child, num_segment, segment_lines, extra);

    //open parent file
    fp_parent = fopen("parent_output.txt", "w");
    if (fp_parent == NULL){
        perror("error\n");
        exit(1);
    }


    for (int i = 0; i < N*request_per_child; i++){

        //producer-consumer
        sem_wait(&(sh_request_ptr->request_full)); 
        sem_wait(&(sh_request_ptr->request_mutex));
        request = sh_request_ptr->buffer[sh_request_ptr->out]; 
        sh_request_ptr->out = (sh_request_ptr->out + 1) % BUFFER_SIZE;
        sem_post(&(sh_request_ptr->request_mutex)); 
        sem_post(&(sh_request_ptr->request_empty)); 


        if(current_segment != request.segment){
            sem_wait(&(sh_request_ptr->print_mutex)); 
            if (current_segment == -1){ 
                printf("Parent - Loading segment %d \n", request.segment); 
                fprintf(fp_parent, "Time:%ld - Loading segment %d\n", time(0), request.segment); 
            } 
            else{ 
                printf("Parent - Replacing segment %d with segment %d \n", current_segment, request.segment); 
                fprintf(fp_parent, "Time:%ld - Replacing segment %d with segment %d \n", time(0), current_segment, request.segment); 
            } 
            sem_post(&(sh_request_ptr->print_mutex));


            //go to the beginning of file
            fseek(fp_input, SEEK_SET, 0);
            //go to request.segment-1 to the last char
            for(int j=0;j<request.segment*segment_lines;j++){
                while((ch = fgetc(fp_input)) != '\n');
            }

            if(extra != 0 )
                if(request.segment == num_segment-1)
                    segment_lines = extra;
            //load the segment
            for(int j=0;j<segment_lines;j++){
                int k =0;
                while((ch = fgetc(fp_input)) != '\n'){
                    segment[j][k] = ch;
                    k++;
                }
                segment[j][k] = '\0';
            }
            current_segment = request.segment;
        }

        //send response to child
        sem_wait(&(sh_response_ptr[request.child_id]->response_parent)); 
        sem_wait(&(sh_request_ptr->print_mutex)); 
        printf("Parent response- Sending line %d of segment %d to child %d \n", request.line, request.segment, request.child_id); 
        sem_post(&(sh_request_ptr->print_mutex)); 
        strcpy(sh_response_ptr[request.child_id]->response_line, segment[request.line]); 
        sem_post(&(sh_response_ptr[request.child_id]->response_child));
    }

    //wait until children finish their proccess
    for (int i=0;i<N;i++)
        wait(NULL); 
    
    fclose(fp_input); 
    fclose(fp_parent); 

    sem_destroy(&(sh_request_ptr->request_mutex));
    sem_destroy(&(sh_request_ptr->request_empty));
    sem_destroy(&(sh_request_ptr->request_full));
    sem_destroy(&(sh_request_ptr->print_mutex));
    for(int i=0;i<N;i++){
        sem_destroy(&(sh_response_ptr[i]->response_parent));
        sem_destroy(&(sh_response_ptr[i]->response_child));
    }

    shmdt(sh_request_ptr);

    shmctl(shm_id, IPC_RMID, 0);

    for(int i=0;i<N;i++) { 
        shmdt(sh_response_ptr[i]); 
        shmctl(shm_response_id[i], IPC_RMID, 0); 
    }

    free(shm_response_id); 

    free(sh_response_ptr);

    for(int i=0;i<segment_lines;i++)
        free(segment[i]); 
    free(segment); 

    return 0;
}