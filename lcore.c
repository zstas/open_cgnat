#include "main.h"

void lcore_worker_init_conf( struct lcore_conf *lcore, uint8_t type, uint8_t id )
{
        lcore->id = rte_lcore_id();
        lcore->type = type;

        char buf[128];

        snprintf( buf, 128, "worker_from_main_%d", id );
	RTE_LOG( INFO, WORKER , "Creating ring %s\n", buf );
        lcore->from_main = rte_ring_create( buf, IPC_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ );

        snprintf( buf, 128, "worker_to_main_%d", id );
	RTE_LOG( INFO, WORKER , "Creating ring %s\n", buf );
        lcore->to_main = rte_ring_create( buf, IPC_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ );
}

void lcore_rx_init_conf( struct lcore_conf *lcore, uint8_t type )
{
        lcore->id = rte_lcore_id();
        lcore->type = type;

        char buf[128];

        snprintf( buf, 128, "rx_from_main_%d", lcore->id );
	RTE_LOG( INFO, LCORE_RX , "Creating ring %s\n", buf );
        lcore->from_main = rte_ring_create( buf, IPC_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ );

        snprintf( buf, 128, "rx_to_main_%d", lcore->id );
	RTE_LOG( INFO, LCORE_RX , "Creating ring %s\n", buf );
        lcore->to_main = rte_ring_create( buf, IPC_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ );
}

struct rte_ring * ring_wait_and_lookup( char * buf )
{
	while( rte_ring_lookup( buf ) == NULL )
	{
		usleep( 300 );
		RTE_LOG( DEBUG, MAIN , "Waiting for ring %s\n", buf );
	}
	return rte_ring_lookup( buf );
}

