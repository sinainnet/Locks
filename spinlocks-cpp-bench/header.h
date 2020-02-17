#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <time.h>
#include <fcntl.h>
#include <errno.h>          // errno
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>       // mmap
#include <sys/time.h>
#include <sys/types.h>

#define IOV_NUM         512

struct Data {
  int state;
};

struct spinlock {
    int locked;
};

#define SPINLOCK_INIT { 0 };

typedef struct main_inputs {
        pid_t pid;
        char *remote_ptr;
        ssize_t buffer_length;
        struct iovec		*local;
	struct iovec		*remote;
} data_input;

#define DATA_SIZE (sizeof(struct Data))


// typedef enum {true, false} bool;

static pthread_barrier_t barrier;
static int numBarriers;

/*
 *  Threads Data Structure to keep track of each one
 */
typedef struct thread_tags {
	pthread_t		thread_id;
	int         		thread_num;
	int			thread_chunks;
	struct iovec		*local;
	struct iovec		*remote;
	data_input 		input;
	struct Data		*shm;
	int 			*lock_count;
	struct spinlock		*lock;
} thread_tracker;

typedef struct thread_return_data {
	struct timespec 	start;
	struct timespec 	finish;
	ssize_t 		nread;
	// bool        		printed;
	// bool 			status;
	int			min_offset;
	int 			max_offset;
} thread_result;

#endif  /* PROTOCOL_H */

#define two			2
#define seven			7
#define ten			10
#define col			1024
#define meg_row			1024
#define gig_row			1048576
#define two_gig_row		(long long int)(two * gig_row)
#define twenty_gig_row		(long long int)(ten * two * gig_row)
#define four_gig_row		(long long int)(two*two_gig_row)
#define seven_gig_row		(long long int)(seven*gig_row)
#define eight_gig_row		(long long int)(two*four_gig_row)
#define fourteen_gig_row	(long long int)(two*seven_gig_row)
#define sixteen_gig_row		(long long int)(two*eight_gig_row)
#define lli			(long long int)
#define gig_size		(unsigned long long int)((unsigned long)gig_row*(unsigned long)col)
#define two_gig_size    	((long)two*(long)gig_size)
#define twenty_gig_size    	((long)two * (long)ten *(long)gig_size)
#define four_gig_size		((long)two * (long)two_gig_size)
#define seven_gig_size		((long)seven * (long)gig_size)
#define eight_gig_size  	((long long)two*(long long)four_gig_size)
#define fourteen_gig_size  	((long long)two*(long long)seven_gig_size)
#define sixteen_gig_size  	((long long)two*(long long)eight_gig_size)
#define shm_writer_file		"Write_finish"
#define shm_reader_file		"Read_finish"
#define shm_file_creat_mod	(O_CREAT | O_RDWR)
#define shm_file_use_mod	(O_RDWR)
#define shm_prov_prot		(PROT_READ | PROT_WRITE) 	// Provider is the 2nd program to run
#define shm_prov_flags		MAP_SHARED			// Provider is the 2nd program to run
#define shm_cons_prot						// Provider is the first program to run
#define shm_cons_flags						// Provider is the first program to run
#define psvm_writer		"process_vm_writev"
#define psvm_reader		"process_vm_readv"
#define one_gig_file  		"results/one_gig.txt"
#define two_gig_file  		"results/two_gig.txt"
#define four_gig_file  		"results/four_gig.txt"
#define seven_gig_file  	"results/seven_gig.txt"
#define eight_gig_file  	"results/eight_gig.txt"
#define fourteen_gig_file  	"results/fourteen_gig.txt"
#define sixteen_gig_file  	"results/sixteen_gig.txt"
#define twenty_gig_file  	"results/twenty_gig.txt"
#define middleware		"./middleware.txt"


void set_cpu_scheduler (int cpu_no, int priority) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu_no, &set);
        if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
        {
                printf("setaffinity error");
                exit(0);
        }
        struct sched_param sch_p;
        sch_p.sched_priority = priority;
        int re = sched_setscheduler(getpid(), SCHED_RR, &sch_p);
        if(re < 0) {
                printf("sched_setscheduler returned error code %d.\n", re);
                exit(0);
        }
        return;
}



void get_inputs (data_input *input_var, int argc, char **argv) {
        if (argc < 3) {
                printf("usage: %s <pid> <mem address> [len]\n", argv[0]);
                printf("  <pid> - PID of process to target\n");
                printf("  <mem> - Memory address to target\n");
                printf("  [len] - Length (in bytes) to dump\n");
                exit(1);
        }

        input_var->pid = strtol(argv[1], NULL, 10);
        printf(" * Launching with a target PID of: %d\n", input_var->pid);

        input_var->remote_ptr = (char *)strtol(argv[2], NULL, 0);
        printf(" * Launching with a target address of 0x%llx\n", (long long unsigned)input_var->remote_ptr);

        input_var->buffer_length = (argc > 3) ? strtol(argv[3], NULL, 10) : 20;
        printf(" * Launching with a buffer size of %lu bytes.\n", input_var->buffer_length);

        return;
}

void build_iovecs(size_t threadNum, data_input *dataInput) {
        // printf("\n1");
        int local_iov_num = (long long int)IOV_NUM;
        // printf("\n1");
        long long int data_len = dataInput->buffer_length/local_iov_num;
        // printf("\n1");

        struct iovec local[local_iov_num];
        // printf("\n1");
        struct iovec remote[local_iov_num];
        // printf("\n1");

        for (int i = 0; i < local_iov_num; i++)
        {
                // printf("\n1");
                char *data = new char[data_len];
                // printf("\n1");
                memset(data, char(((int)'a') + i), data_len);
                // printf("\n1");
                local[i].iov_base = data;
                // printf("\n1");
                local[i].iov_len = data_len;

                // printf("\n1");
                remote[i].iov_base = dataInput->remote_ptr + i*data_len*sizeof(char);
                // printf("\n1");
                remote[i].iov_len = data_len;
                // printf("\n1");
	}
        return;
}