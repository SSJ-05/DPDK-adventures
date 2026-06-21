// refactor tx_burst // 21.06.26// ZeroK

#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ethdev.h>


static constexpr std::uint16_t BUFFER_SIZE { 1024 };
static constexpr std::uint16_t CACHE_SIZE  { 256 };
static constexpr std::uint32_t PORT_ID     { 0 };

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
                ports, PORT_ID);


    rte_eth_dev_info info;
    int ret = rte_eth_dev_info_get (PORT_ID, &info); 
    if (ret < 0) rte_exit (EXIT_FAILURE, "FAILED TO GET ETH DEV INFO.\n");
    std::printf ("\nAVAILABLE H/W:\n"
                    "\tports: %u\n"
                    "\ttx_queues: %u\n"
                    "\trx_queues: %u\n"
                    "\tdriver_name: %s\n",
                    ports, info.max_tx_queues, info.max_rx_queues, info.driver_name
                );
}


void create_mempool (std::uint16_t buf_size,
                     std::uint16_t cache_size, 
                     const char* name) {
    
    rte_mempool* pool = 
        rte_pktmbuf_pool_create (
                name,
                buf_size,
                cache_size,
                0,
                RTE_MBUF_DEFAULT_BUF_SIZE,
                rte_socket_id()
            );
    if (!pool) rte_exit (EXIT_FAILURE, "POOL CREATION FAILED\n");
}


void create_rx_q (std::uint16_t buf_sz) {
    
    auto* rx_pool = 
        create_mempool (buf_sz, CACHE_SIZE, "rx_pool");
    
    int ret = 
        rte_eth_rx_queue_setup (
            PORT_ID,
            0,
            buf_sz,
            rte_socket_id(),
            nullptr,
            rx_pool
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "RX QUEUE CREATION FAILED\n");
}

void create_tx_q (std::uint16_t buf_sz) {
    
    int ret = 
        rte_eth_tx_queue_setup (
            PORT_ID,
            0,
            buf_sz,
            rte_socket_id(),
            nullptr
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "TX QUEUE CREATION FAILED\n");
}



void start_port () {

    int ret = rte_eth_dev_start (PORT_ID);


}


int shutdown () {}

int main (int argc, char** argv) {

    eal_init (argc, argv);

    discover_port ();

    create_mempool (BUFFER_SIZE, CACHE_SIZE, "pool");

    return EXIT_SUCCESS;
}
