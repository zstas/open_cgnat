#include "main.h"

int lcore_tx_per_port( void * p )
{
	uint16_t port = *(uint16_t *)p;

	void *packets[ TX_BURST ];
	uint16_t rx_pkts;
	char buf[128];
	RTE_LOG( INFO, LCORE_TX, "Started on core %d for port %u\n", rte_lcore_id(), port );

	memset( buf, 0, 128);
	snprintf( buf, 128, "port_%d_tx", port );

	struct rte_ring *ring;
	RTE_LOG( INFO, LCORE_TX, "Creating ring %s\n", buf );
	ring = rte_ring_create( buf, TX_RING, rte_socket_id(), RING_F_SC_DEQ );

	while( 1 )
	{
		rx_pkts = rte_ring_dequeue_burst( ring, packets, TX_BURST, NULL );
		if( likely( rx_pkts > 0 ) )
		{
			rte_eth_tx_burst( port, 0, (struct rte_mbuf **)packets, rx_pkts );
		}
	}
}

int lcore_tx( __attribute__((unused)) void * p )
{
	uint16_t port;
	uint16_t nb_ports = rte_eth_dev_count_avail();

	void *packets[ TX_BURST ];
	uint16_t rx_pkts;
	char buf[128];

	RTE_LOG( INFO, LCORE_TX, "Started on core %d for all ports\n", rte_lcore_id() );
	struct rte_ring *ring[ TX_MAX_PORTS ];

	for( port = 0; port < nb_ports; port++ )
	{
		memset( buf, 0, 128);
		snprintf( buf, 128, "port_%d_tx", port );

		RTE_LOG( INFO, LCORE_TX, "Creating ring %s\n", buf );
		ring[ port ] = rte_ring_create( buf, TX_RING, rte_socket_id(), RING_F_SC_DEQ );

		if( ring[ port ] == NULL )
			rte_exit( EXIT_FAILURE, "Cannot created a ring %s\n", buf );
	}

	while( 1 )
	{
		for( port = 0; port < nb_ports; port++ )
		{
			rte_prefetch0( packets );
			rx_pkts = rte_ring_dequeue_burst( ring[ port ], packets, TX_BURST, NULL );
			if( likely( rx_pkts > 0 ) )
			{
				rte_eth_tx_burst( port, 0, (struct rte_mbuf **)packets, rx_pkts );
			}
		}
	}

	return 0;
}

