// helloworld of dpdk// 14.06.26// ZeroK
// source: doc.dpdk.org/guides.html

/*  what this program does
 *  linux thread -> DPDK lcore abstraction -> launch func on lcores -> wait for their completion
 *  thats DPDK equivalent of std::thread (pthread_create)
 *  but here DPDK wants to own the worker threads by itself
 * */


#include <cstdio>
#include <cstdlib>

extern "C" {
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_launch.h>
}


// after EAL init, application is ready to launch lcore_hello func on each lcore
static int lcore_hello (void*) {
    
    unsigned id = rte_lcore_id ();
    std::printf ("hello from core %u\n", id);

    return 0;
}

// for detailed info
static int lcore_info (void*) {
    
    std::printf ("[worker] core: %u  socket: %u\n",
                rte_lcore_id(), rte_socket_id()
                );

    return 0;
}


int main (int argc, char** argv) {

    // init EAL (Environment Abstraction Layer)
    int ret = rte_eal_init (argc, argv);
    if (ret < 0) {
        // rte_panic ("Cannot init EAL\n");
        rte_exit (EXIT_FAILURE, "Error with EAL init\n");
        // std::fprintf (stderr, "Error with EAL init\n");
    }


    // functions go inside main() - only declarations in global scope
    // print hello from each logical core
    unsigned lcore_id;
    // RTE_LCORE_FOREACH_WORKER (lcore_id) {
    //
    //     std::printf ("Hello from core %u\n", lcore_id);
    // }
    
    // all worker cores execute concurrently
    // and launched asynchronously
    RTE_LCORE_FOREACH_WORKER (lcore_id) {

        rte_eal_remote_launch (lcore_hello, nullptr, lcore_id);
    }       

    lcore_hello (nullptr);


    // after launching workers wait
    // otherwise main() exits without workers launching
    rte_eal_mp_wait_lcore();


    // cleanup
    rte_eal_cleanup();

    return EXIT_SUCCESS;
}
