// TX/RX queues intro// 20.06.26// ZeroK

/* Analogy: train
 * alloc    :   get wagon
 * append   :   load cargo
 * mtod     :   inspect cargo
 * free     :   return wagon
 * NIC      :   railway stations
 *
 * Notes:
 * we need a place to exchange work from CPU to NIC and vice-versa
 * TX queue : CPU puts packets here (producer), NIC processes them (consumer)
 * RX queue : NIC puts packets here (producer), CPU processes them (consumer)
 *
 * */

#include <cstdlib>
#include <cstdio>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ethdev.h>



int main (int argc, char** argv) {

    int ret;
    if ((ret = rte_eal_init (argc, argv)) < 0) rte_exit (EXIT_FAILURE, "EAL INIT FAILED.\n");

    unsigned nr_ports = rte_eth_dev_count_avail();
    if (nr_ports == 0) rte_exit (EXIT_FAILURE, "NO PORTS AVAILABLE.\n");
    std::printf ("\nports available: %u\n", nr_ports);

    unsigned port_id = 0;   // 0 == physical NIC, RTL8111 here
    if (nr_ports != 1) 
        std::printf ("WARNING: ports available : %u, we only use port %u\n",
                        nr_ports, port_id);

    rte_eth_dev_configure(port_id);
    rte_eth_tx_queue_setup();
    rte_eth_rx_queue_setup();
    rte_eth_dev_start();


    rte_eth_dev_stop();
    rte_eal_cleanup();

    return EXIT_SUCCESS;
}
