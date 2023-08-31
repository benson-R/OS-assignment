
#include <stdio.h>
#define __USE_GNU
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>

pthread_barrier_t barrier;

typedef struct
{
   pthread_t thread_id;
   int thread_num;
   int sched_policy;
   int sched_priority;
   float time_wait;
}thread_info_t;

void *child_thread(void *arg)
{
    int taskid;
    double time_waiting;
    thread_info_t *my_data;
    my_data = (thread_info_t *) arg;
    taskid = my_data->thread_id;
    time_waiting = my_data->time_wait;
    pthread_barrier_wait(&barrier);

    for(int i=0; i< 3;i++)
    {
        printf("Thread %d is running\n", taskid);
        struct timeval start;
        struct timeval end;
        double start_time, end_time;

        gettimeofday(&start,NULL);
        start_time = (start.tv_sec*1000000+(double)start.tv_usec);
        start_time/=1000000;

        while(1)
        {
            gettimeofday(&end,NULL);
            end_time = (end.tv_sec*1000000+(double)end.tv_usec);
            end_time/=1000000;
            if (end_time > start_time+time_waiting)
                break;
        }  
        sched_yield();
    }
    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    int parse= 0;
    int num_threads = atoi(argv[2]);
    char *string = "n:t:s:p:";
    const char* d = ",";
    char *p, *Sch1, *Sch2;
    char *pthread_policy[num_threads];
    int pthread_priority_int[num_threads];
    float wait_time;

    while((parse = getopt(argc, argv, string))!=-1)
    {
        /* 1. Parse program arguments */
        switch (parse) {
            case 'n':
                break;
            case 't':
                wait_time = atof(optarg);
                break;
            case 's':
                Sch1=optarg;
                p = strtok(Sch1, d);
                int i =0;
                while(p!=NULL)
                {
                    pthread_policy[i]=p;
                    p = strtok(NULL, d);
                    i++;
                }
                break;
            case 'p':
                Sch2=optarg;
                p = strtok(Sch2, d);
                int j=0;
                while(p!=NULL)
                {
                    pthread_priority_int[j] = atoi(p);
                    p = strtok(NULL, d);
                    j++;
                }
                break;
        }
    }

    thread_info_t thread_data_array[num_threads];
    pthread_t child_thread_id[num_threads];
    struct sched_param param[num_threads];
    pthread_attr_t attr[num_threads];
    int *taskids[num_threads];
    int rc, rt, rb, policy;
    char *str1 = "FIFO";
    char *str2 = "NORMAL";
    int setpolicy =0 ;
    int max_priority,min_priority;

    /*2. Set CPU affinity*/
    int cpu_id = 3; // set thread to cpu3
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    pthread_barrier_init(&barrier, NULL, num_threads+1);

    /*3. Set the attributes to each thread*/
    for(int i=0;i<num_threads;i++)
    {
        thread_data_array[i].thread_id=i;
        thread_data_array[i].time_wait=wait_time;
        pthread_attr_init(&attr[i]); 
        pthread_attr_setinheritsched(&attr[i],PTHREAD_EXPLICIT_SCHED); 
        pthread_attr_getinheritsched(&attr[i],&policy); 

        if(strcmp(pthread_policy[i],str2) == 0)
        {
            setpolicy = 0;
        }
        else
        {
            setpolicy = 1;
        }
        pthread_attr_setschedpolicy(&attr[i],setpolicy);
        pthread_attr_getschedpolicy(&attr[i],&policy);

        param[i].sched_priority=pthread_priority_int[i];
        if(pthread_priority_int[i]!=-1)
        {
            pthread_attr_setschedparam(&attr[i],&param[i]);
        }

        /*4. Create <num_threads> worker threads */
        pthread_create(&child_thread_id[i], &attr[i], child_thread, (void *) 
        &thread_data_array[i]);
    }

    /*5. Start all threads at once*/
    pthread_barrier_wait(&barrier);
    pthread_exit(NULL);
    return 0;
    
}