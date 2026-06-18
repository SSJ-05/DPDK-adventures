// dpdk practice session// 18.06.26// ZeroK

#include <cstdio>
#include <cstdlib>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

struct Tick {
    uint64_t seq;
    double   bid;
    double   ask;
};

/* Workflow:
 * Tick on stack -> memcpy into mbuf payload -> mtod/read by Tick*
 * */


int main (int argc, char** argv) {

    // EAL
    int ret;
    if ((ret = rte_eal_init (argc, argv)) < 0) rte_exit (EXIT_FAILURE, "failed EAL init.\n");
    std::printf ("\n");

    // mempool
    constexpr int NUM_MBUFS     { 8192 };
    constexpr int CACHE_SIZE    { 256 };

    rte_mempool* pool = rte_pktmbuf_pool_create (
                "z_pool",
                NUM_MBUFS,
                CACHE_SIZE,
                0, 
                RTE_MBUF_DEFAULT_BUF_SIZE,
                rte_socket_id()
            );
    if (!pool) rte_exit (EXIT_FAILURE, "failed mempool init.\n");

    // allocate mbuf
    auto* pkt = rte_pktmbuf_alloc (pool);
    if (!pkt) rte_exit (EXIT_FAILURE, "failed mbuf allocation.\n");

    // std::printf ("mbuf size : %zu\n", sizeof(rte_mbuf));
    // std::printf ("pkt_addr  : %p\n",  static_cast<void*>(pkt));
    // std::printf ("buf_len   : %u\n",  pkt->buf_len);
    // std::printf ("pkt_len   : %u\n",  pkt->pkt_len);
    // std::printf ("data_len  : %u\n",  pkt->data_len);
    // std::printf ("data_off  : %u\n",  pkt->data_off);
    

    // append
    // char* p = rte_pktmbuf_append (pkt, 64);
    // __builtin_memset (p, 'Z', 64);
    
    // std::printf ("\nafter append:\n");
    // std::printf ("buf_len   : %u\n",  pkt->buf_len);
    // std::printf ("pkt_len   : %u\n",  pkt->pkt_len);
    // std::printf ("data_len  : %u\n",  pkt->data_len);
    // std::printf ("data_off  : %u\n",  pkt->data_off);
    // std::printf ("data      : %s\n",  p);


    // mtod
    // std::printf ("\nafter mtod:\n");
    // char* data = rte_pktmbuf_mtod (pkt, char*);
    // std::printf ("buf_addr : %p\n", pkt->buf_addr);
    // std::printf ("data     : %s\n", data);
    
    // unsigned diff = (char*)data - (char*)pkt->buf_addr;     // data_off in a nutshell
    // std::printf ("diff : %u\n", diff);


    // tick mbuf
    Tick tick = { .seq = 100, .bid = 100.05, .ask = 101.05 };
    void* dst = rte_pktmbuf_append (pkt, sizeof(Tick));
    __builtin_memcpy (dst, &tick, sizeof(Tick));

    // payload start + cast (Tick*)
    auto* start = rte_pktmbuf_mtod (pkt, Tick*);

    std::printf ("seq: %lu  " "bid: %.2f  " "ask: %.2f\n",
                    start->seq, start->bid, start->ask );


    // return same addr
    // append(dst) and mtod(start) both return start of payload
    std::printf ("payload addr : %p\n", dst);
    std::printf ("mtod addr    : %p\n", start);

    rte_pktmbuf_free (pkt);

    rte_eal_cleanup();

    return EXIT_SUCCESS;
}
