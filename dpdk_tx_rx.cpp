// TX/RX queues intro// 20.06.26// ZeroK
// basic NIC bring-up and tx_burst prototype

/* Analogy: train
 * mbuf             :   container
 * alloc            :   get wagon
 * append           :   load cargo in container
 * mtod             :   inspect cargo
 * free             :   return wagon
 * NIC/port 0       :   railway station
 * rx queue         :   receiving lane
 * tx queue         :   transmitting lane
 * descriptor ring  :   loading slots in lane 
 *
 *
 * Workflow:
 * 1. setup eal env
 * 2. get NIC h/w info
 * 3. configure ports (how many lanes required)
 * 4. setup desc rings (tx and rx)
 *     **for rx queue - create a mempool rx_pool to store incoming packets from NIC
 * 5. dev start: create tx_pool, alloc mbuf, append, memcpy tick into mbuf
 * 6. dev stop
 * 7. dev close: close port 0
 * 8. eal cleanup
 * 9. exit
 *
 *
 * Notes:
 * we need a place to exchange work from CPU to NIC and vice-versa
 * TX queue : CPU puts packets here (producer), NIC processes them (consumer) -> CPU transmits
 * RX queue : NIC puts packets here (producer), CPU processes them (consumer) -> CPU receives
 * NIC doesnt store packets. It stores references/pointers/descriptors to mbufs.
 * TX/RX are descriptor rings allocated during queue setup
 * */


#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "tick.hpp"     // 64 byte tick


int main (int argc, char** argv) {

    static_assert (sizeof(Tick) == 64);

    // 1.
    int ret;
    if ((ret = rte_eal_init (argc, argv)) < 0) rte_exit (EXIT_FAILURE, "EAL INIT FAILED.\n");

    unsigned nr_ports = rte_eth_dev_count_avail();
    if (nr_ports == 0) rte_exit (EXIT_FAILURE, "NO PORTS AVAILABLE.\n");
    std::printf ("\nports available: %u\n", nr_ports);

    unsigned port_id = 0;   // 0 == physical NIC, RTL8111 here
    if (nr_ports != 1) 
        std::printf ("WARNING: ports available : %u, we only use port %u\n",
                      nr_ports, port_id);


    // 2. info about hardware capabilities of NIC
    rte_eth_dev_info info;
    ret = rte_eth_dev_info_get (port_id, &info);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV INFO GET FAILED.\n");

    std::printf ("max tx_queues: %u " " max rx_queues: %u " " driver_name: %s\n",
            info.max_tx_queues, info.max_rx_queues, info.driver_name);


    // 3. 
    constexpr std::uint8_t RX_Q   { 1 };
    constexpr std::uint8_t TX_Q   { 1 };

    rte_eth_conf port_conf  {};
    ret = rte_eth_dev_configure (
            port_id,
            RX_Q,              // rx queues
            TX_Q,              // tx queues
            &port_conf);
    if (ret < 0) rte_exit (EXIT_FAILURE, "PORT CONFIGURE FAILED\n");


    // 4. 
    constexpr std::uint16_t TX_DESC     { 1024 };   // 1024 slots in loading lanes
    constexpr std::uint16_t RX_DESC     { 1024 };
    constexpr std::uint16_t CACHE_SIZE  { 256 };

    ret = rte_eth_tx_queue_setup (
            port_id,
            0,
            TX_DESC,            // 1024 desc slots in queue pointing to mbufs
            rte_socket_id(),
            nullptr             // default config for tx queue
        );
    // std::printf ("tx setup ret: %d\n", ret);    // ret == 0 -> success
    if (ret < 0) rte_exit (EXIT_FAILURE, "TX_QUEUE SETUP FAILED\n");

    
    auto* rx_pool = rte_pktmbuf_pool_create (
            "rx_pool",
            RX_DESC,            // no. of mbufs
            CACHE_SIZE,                
            0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id()
        );
    if (!rx_pool) rte_exit (EXIT_FAILURE, "RX_POOL CREATION FAILED.\n");
    
    ret = rte_eth_rx_queue_setup (
            port_id,
            0, 
            RX_DESC,
            rte_socket_id(),
            nullptr,
            rx_pool            // mempool
        );
    // std::printf ("rx setup ret: %d\n", ret);
    if (ret < 0) rte_exit (EXIT_FAILURE, "RX_QUEUE SETUP FAILED\n");


    /*****************************************************************************/
    // 5. start device
    ret = rte_eth_dev_start (port_id);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV START FAILED\n");
    // std::printf ("dev start ret: %d\n", ret);

    // create tick
    Tick tick = { .seq = 10, .timestamp_ns = 0, 
                  .bid = 100.05, .ask = 101.05,
                  .bid_qty = 20, .ask_qty = 30, .symbol = "AAPL" };

    // create mempool
    auto* tx_pool = rte_pktmbuf_pool_create (
            "tx_pool",
            TX_DESC,
            CACHE_SIZE, 
            0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id()
        );
    if (!tx_pool) rte_exit (EXIT_FAILURE, "TX_POOL CREATION FAILED.\n");

    // alloc mbuf
    auto* pkt = rte_pktmbuf_alloc (tx_pool);       
    if (!pkt) rte_exit (EXIT_FAILURE, "MBUF ALLOC FAILED.\n");

    // append
    void* dst = rte_pktmbuf_append (pkt, sizeof(Tick));
    __builtin_memcpy (dst, &tick, sizeof(Tick));   


    // create array - coz DPDK processe pkts in batches
    rte_mbuf* tx_pkts [1];
    tx_pkts [0] = pkt;

    // transmit/send pkts in bursts
    // if sent == 1, NIC accpted the pkt
    // if sent == 0, NIC accepted nothing and pkt is freed
    std::uint16_t sent = 
        rte_eth_tx_burst (
            port_id,
            0,
            tx_pkts,
            1
        );
    if (sent == 1) std::printf ("\n**tx_burst success**\n" "sent %u packets\n\n", sent);
    else if (sent == 0) {
        std::printf ("\n**tx_burst failed**\n\n");
        rte_pktmbuf_free (pkt);
    }


    // 6. stop the device
    ret = rte_eth_dev_stop (port_id);
    // std::printf ("dev stop ret: %d\n", ret);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV STOP FAILED\n");
    /*****************************************************************************/



    // 7. close
    ret = rte_eth_dev_close (port_id);
    // std::printf ("dev stop ret: %d\n", ret);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV CLOSE FAILED\n");


    // 8. cleanup
    ret = rte_eal_cleanup();
    // std::printf ("eal cleanup ret: %d\n", ret);
    if (ret < 0) rte_exit (EXIT_FAILURE, "EAL CLEANUP FAILED\n");

    return EXIT_SUCCESS;
}
