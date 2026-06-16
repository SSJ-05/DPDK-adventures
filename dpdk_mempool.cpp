// mempool in dpdk// 16.06.26// ZeroK

// workflow: 
//      EAL -> create mempool -> allocate mbuf -> print addr -> free mbuf
//
// Analogy:
//      HugePage    :   Warehouse
//      mempool     :   shelves
//      mbufs       :   boxes
//
// **Observations:
//      * Purpose of pt.6 - mbuf1 freed - that block is reused for mbuf2
//      * same address o/p for both mbuf1 and mbuf2
//      * pre-allocated memory at env startup - mem address of pool remains the same
//      * consecutive allocations are placed at a gap of 2368 bytes


#include <cstdio>
#include <cstdlib>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_launch.h>
#include <pthread.h>
#include <sched.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>


int main (int argc, char** argv) {

    // 1. EAL
    int ret = rte_eal_init (argc, argv);
    if (ret < 0) rte_exit (EXIT_FAILURE, "Failed to EAL init.\n");


    // 2. create mempool - mbufs are stored in mempool
    constexpr int NUM_MBUFS     { 8192 };
    constexpr int CACHE_SIZE    { 256 };

    rte_mempool* pool = 
        rte_pktmbuf_pool_create (
                "MBUF_POOL",
                NUM_MBUFS,
                CACHE_SIZE,
                0,
                RTE_MBUF_DEFAULT_BUF_SIZE,
                rte_socket_id()
            );

    if (!pool) rte_exit (EXIT_FAILURE, "mempool creation failed.\n");

    // check available memory
    std::printf ("\navailable: %u\n", rte_mempool_avail_count(pool));



    // 3. allocate one mbuf from mempool
    rte_mbuf* pkt = rte_pktmbuf_alloc (pool);

    if (!pkt) rte_exit (EXIT_FAILURE, "mbuf allocation failed.\n");


    // 4. print
    std::printf ("\npool: %p  mbuf: %p\n", (void*)pool, (void*)pkt);

    std::printf ("available: %u\n", rte_mempool_avail_count(pool));

    // 5. free mbuf
    // rte_pktmbuf_free (pkt);     

    
    // 6. allocate again, print and free it
    auto* pkt2 = rte_pktmbuf_alloc (pool);
    if (!pkt2) rte_exit (EXIT_FAILURE, "mbuf allocation failed.\n");

    std::printf ("pool: %p  mbuf: %p\n", (void*)pool, (void*)pkt2);

    std::printf ("available: %u\n", rte_mempool_avail_count(pool));


    // size check
    std::printf ("size of mbuf: %zu\n", sizeof(rte_mbuf));
    std::printf ("buf len: %u\n", pkt->buf_len);
    // size + buf_len + alignment + padding = 2368 bytes


    // free mbufs
    rte_pktmbuf_free (pkt);
    rte_pktmbuf_free (pkt2);
    


    rte_eal_cleanup();

    return EXIT_SUCCESS;
}
