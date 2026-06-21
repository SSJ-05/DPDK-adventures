// refactor tx_burst v3 // 21.06.26// ZeroK
// implemented basic packet transmit pipeline in dpdk

/* workflow:
 *      init EAL
 *      check hardware capabilities
 *      configure port and queues
 *      create mempools (tx and rx)
 *      allocate and append mbufs with ticks
 *      transmit mbufs in bursts via tx_burst
 *      check mempool consumption
 *
 * observations:
 *      output showed available = 3073, in use = 1023
 *      this strongly suggests PMD is holding onto to pkts
 *      rather than immediately returning them to mempool
 *
 *      dpdk documentation stated that some NICs (RTL_8169 in this case)
 *      retain mbufs in TX ring
 *      and only release them in batches after thresholds are crossed or
 *      descriptors are needed again
 * */


#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "tick.hpp"
static_assert (sizeof(Tick) == 64, "Tick should be 64 bytes.\n");


namespace cfg {

    constexpr std::uint16_t  DESC_SIZE    { 1024 };
    constexpr std::uint16_t  BUFFER_SIZE  { 4096 };
    constexpr std::uint16_t  CACHE_SIZE   { 256 };
    constexpr std::uint16_t  BURST_SIZE   { 32 };
    constexpr std::uint16_t  PORT_ID      { 0 };
    constexpr std::uint16_t  RX_Q         { 1 };
    constexpr std::uint16_t  TX_Q         { 1 };

}


Tick tick_generate () {

    static std::uint64_t _seq = 0;

    return Tick {
        .seq = ++_seq,
        .timestamp_ns = 0,
        .bid = 100.05,
        .ask = 101.00,
        .bid_qty = 20,
        .ask_qty = 30,
        .symbol = "AAPL"
    };
}

int eal_init (int argc, char** argv) {

    int ret;
    if ((ret = rte_eal_init (argc, argv)) < 0)
        rte_exit (EXIT_FAILURE, "EAL INIT FAILED\n");

    return ret;
}

void discover_port () {
    
    unsigned ports = rte_eth_dev_count_avail();
    
    if (ports == 0) rte_exit (EXIT_FAILURE, "NO PORTS AVAILABLE.\n");

    if (ports != 1)
        std::printf ("\nPorts available: %u, "  "We use port %d\n", 
                ports, cfg::PORT_ID);


    rte_eth_dev_info info;
    int ret = rte_eth_dev_info_get (cfg::PORT_ID, &info); 
    if (ret < 0) rte_exit (EXIT_FAILURE, "FAILED TO GET ETH DEV INFO.\n");
    std::printf ("\nAVAILABLE H/W:\n"
                    "\tports: %u\n"
                    "\ttx_queues: %u\n"
                    "\trx_queues: %u\n"
                    "\tdriver_name: %s\n",
                    ports, info.max_tx_queues, info.max_rx_queues, info.driver_name
                );
}


rte_mempool* create_mempool (std::uint16_t buf_size,
                            std::uint16_t cache_size, 
                            const char* name) {
    
    auto* pool = 
        rte_pktmbuf_pool_create (
                name,
                buf_size,
                cache_size,
                0,
                RTE_MBUF_DEFAULT_BUF_SIZE,
                rte_socket_id()
            );
    if (!pool) rte_exit (EXIT_FAILURE, "POOL CREATION FAILED\n");

    return pool;
}


void create_rx_q (std::uint16_t desc_sz) {
    
    auto* rx_pool = 
        create_mempool (cfg::BUFFER_SIZE, cfg::CACHE_SIZE, "rx_pool");
    
    int ret = 
        rte_eth_rx_queue_setup (
            cfg::PORT_ID,
            0,
            desc_sz,
            rte_socket_id(),
            nullptr,
            rx_pool
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "RX QUEUE CREATION FAILED\n");
}

void create_tx_q (std::uint16_t desc_sz) {
    
    int ret = 
        rte_eth_tx_queue_setup (
            cfg::PORT_ID,
            0,
            desc_sz,
            rte_socket_id(),
            nullptr
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "TX QUEUE CREATION FAILED\n");
}

void port_config () {
    
    rte_eth_conf port_conf {};
    int ret = 
        rte_eth_dev_configure (
            cfg::PORT_ID,
            cfg::RX_Q,    // rx queues
            cfg::TX_Q,    // tx queues
            &port_conf
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "PORT CONFIGURE FAILED\n");
}


void start_port () {

    int ret = rte_eth_dev_start (cfg::PORT_ID);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV START FAILED\n");

}

std::uint16_t send_burst (rte_mempool* tx_pool) {

    rte_mbuf* tx_pkts [cfg::BURST_SIZE];

    // generate 32 ticks
    // alloc and append 32 mbufs
    // memcpy those ticks into mbufs
    // then place the mbufs ref in tx_pkts array
    // all in one loop
    for (auto i {cfg::BURST_SIZE}; i-- > 0;) 
    {
        Tick tick = tick_generate();

        auto* pkt = rte_pktmbuf_alloc (tx_pool);
        if (!pkt) rte_exit (EXIT_FAILURE, "MBUF ALLOC FAILED\n");

        void* dst = rte_pktmbuf_append (pkt, sizeof(Tick));
        if (!dst) rte_exit (EXIT_FAILURE, "RTE PKTMBUF APPEND FAILED\n");
        __builtin_memcpy (dst, &tick, sizeof(tick));

        tx_pkts [i] = pkt;
    }


    std::uint16_t sent = 
        rte_eth_tx_burst (
            cfg::PORT_ID,
            0,
            tx_pkts,
            cfg::BURST_SIZE
        );


    // if sent < BURST_SIZE)
    for (std::uint16_t i {sent}; i < cfg::BURST_SIZE; ++i)
        rte_pktmbuf_free (tx_pkts[i]);

    if (sent == 0) {
        std::printf ("TX_BURST FAILED\n");
    }
    else if (sent >= 1) std::printf ("sent: %u / %u\n",
                                    sent, cfg::BURST_SIZE);

    return sent;

}


void shutdown () {
    
    int ret = rte_eth_dev_stop (cfg::PORT_ID);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV STOP FAILED\n");

    ret = rte_eth_dev_close (cfg::PORT_ID);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV CLOSE FAILED\n");

    ret = rte_eal_cleanup();
    if (ret < 0) rte_exit (EXIT_FAILURE, "RTE EAL CLEANUP FAILED\n");
}


int main (int argc, char** argv) {

    int args_consumed = eal_init (argc, argv);
    argc -= args_consumed;
    argv += args_consumed;

    discover_port ();

    port_config ();

    create_rx_q (cfg::DESC_SIZE);
    create_tx_q (cfg::DESC_SIZE);

    start_port ();

    // tx_pool lives here - pass it to send_tick 
    rte_mempool* tx_pool = 
        create_mempool (cfg::BUFFER_SIZE, cfg::CACHE_SIZE, "tx_pool");

    std::printf ("\navailable mempool: %u\n"
                 "in use mempool   : %u\n\n",
                 rte_mempool_avail_count (tx_pool),
                 rte_mempool_in_use_count (tx_pool));

    
    for (auto i {cfg::BURST_SIZE}; i-- > 0;) 
        send_burst (tx_pool);

    std::printf ("\navailable mempool: %u\n"
                 "in use mempool   : %u\n\n",
                 rte_mempool_avail_count (tx_pool),
                 rte_mempool_in_use_count (tx_pool));

    // rte_mempool_free (tx_pool);
    shutdown ();

    return EXIT_SUCCESS;
}
