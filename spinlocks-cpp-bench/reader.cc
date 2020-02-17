// #define _GNU_SOURCE
#include <iostream>
using namespace std;
// #include <unistd.h>
// #include <string.h>
#include <sys/uio.h>
// #include <stdatomic.h>
#include <sys/resource.h>

#include "./header.h"

#define THREADS		2
#define data_len        gig_size


int main(int argc, char **argv) {
        // Changing the process scheduling queue into 
        // real-time and set its priority using <sched.h>.
        set_cpu_scheduler(1,99);

        char    *data = new char[data_len],
                *executor_data = new char[512];

        // // Create Shared Memory
        // struct Data * volatile shm = (struct Data* volatile)shm_builder( \
        //         shm_file_creat_mod, shm_prov_prot, \
        //         shm_prov_flags, shm_writer_file);
        // atomic_store(&shm->state, 0);

        // cout << "reader: sudo ./main " <<  getpid() << " " << data << " " << data_len << endl;
        printf("reader: sudo ./main %d %p %llu\n", getpid(), data, data_len);
        
        // while (__sync_val_compare_and_swap(&shm->state, THREADS, 0) != THREADS);
        while (1);
        
        /* remove the shared memory object */
        // munmap(shm, 1);     
        // shm_unlink("Write_finish");  
        return 0;
}
