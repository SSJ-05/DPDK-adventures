// helloworld of dpdk// 15.06.26// ZeroK

/* experiment 1: compile with 
 *                  g++ -std=c++23 $(pkg-config --cflags --libs libdpdk) -o dh2
 *                  sudo ./dh2
 *
 * experiment 2: limit lcores (-l provides list of lcores that can participate)
 *               compile with
 *                  sudo ./dh2 -l 0-3
 *
 * experiment 3: zero lcores - RTE_LCORE_FOREACH_WORKER doesnt iterate
 *               means lcore world is dependent on EAL args
 *               compile with
 *                  sudo ./dh2 -l 0
 *
 * experiment 4: compile with
 *                  sudo ./dh2 -l 4-7
 *               **lowest number (i.e. 4 in this case) is chosen as main_lcore
 *               **5, 6, 7 are worker lcores 
 *
 * experiment 5: compile with
 *                  suod ./dh2 -l 15
 *               **(when total cores are 16 and available lcores are 15)
 *               **prints only one output - zero iteration - since no workers exists
 *               **lcore_info (nullptr) executes - printing exactly one line
 *
 * experiment 6: compile with
 *                  sudo ./dh2 -l 16
 *               **invalid core id (0-15 total 16 cores - off by one error)
 * */


#include <cstdio>
#include <cstdlib>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_launch.h>
#include <pthread.h>
#include <sched.h>



// map lcore to pthread and cpu cores
// lcores mostly map 1:1 with cpu cores
static int lcore_info (void*) {
    
    auto tid = pthread_self();

    std::printf ("[worker] core: %u  socket: %d  thread: %lu  cpu: %d\n",
                    rte_lcore_id(), 
                    rte_socket_id(),
                    static_cast<unsigned long>(tid),
                    sched_getcpu()
                );

    return 0;
}


int main (int argc, char** argv) {

    // 1. init EAL (Environment Abstraction Layer)
    int ret = rte_eal_init (argc, argv);
    if (ret < 0) {
        rte_exit (EXIT_FAILURE, "Error with EAL init\n");
    }


    std::printf ("[main] main_lcore: %u  current_lcore: %u  socket: %d  cpu: %d\n",
                    rte_get_main_lcore(), 
                    rte_lcore_id(),
                    rte_socket_id(),
                    sched_getcpu()
                );


    // 2. print hello from each logical core
    unsigned lcore_id;
    RTE_LCORE_FOREACH_WORKER (lcore_id) {

        rte_eal_remote_launch (lcore_info, nullptr, lcore_id);
    }       

    lcore_info (nullptr);   // refer experiment 5 above


    // 3. after launching workers wait
    // otherwise main() exits without workers launching
    rte_eal_mp_wait_lcore();


    // 4. cleanup
    rte_eal_cleanup();


    return EXIT_SUCCESS;
}
