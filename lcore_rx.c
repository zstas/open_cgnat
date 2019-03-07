#include "main.h"

extern struct rte_mempool *mbuf_pool;
extern struct app_config global_config;
extern __thread __typeof__( struct lcore_conf ) lcore;

void
rx_process_messages( struct worker_ring_conf *wrks )
{
	struct ipc_msg *msg;

	if( rte_ring_dequeue( lcore.from_main, ( void ** )&msg ) != 0 )
		return;

	RTE_LOG( INFO, LCORE_RX , "Got a ipc message with type 0x%x\n", msg->type );
	switch( msg->type )
	{
		case IPC_RX_START_WORKER:
			RTE_LOG( INFO, LCORE_RX, "Worker %d enabled\n", msg->data[0] );
			wrks[ msg->data[0] ].enabled = 1;
			msg->type = IPC_RX_STARTED_WORKER;
			rte_ring_enqueue( lcore.to_main, (void *)msg );
			break;
		case IPC_RX_STOP_WORKER:
			RTE_LOG( INFO, LCORE_RX, "Worker %d disabled\n", msg->data[0] );
			wrks[ msg->data[0] ].enabled = 0;
			msg->type = IPC_RX_STOPPED_WORKER;
			rte_ring_enqueue( lcore.to_main, (void *)msg );
			break;
		default:
			RTE_LOG( INFO, LCORE_RX , "Lcore RX %d got unknown message with type 0x%x\n", lcore.id, msg->type );
			rte_free( msg );
        }
}


int lcore_rx_per_port( void * p )
{
	uint16_t port = *(uint16_t *)p;

	RTE_LOG( INFO, LCORE_RX, "Started on core %d for port %u\n", rte_lcore_id(), port );
	//sleep( 3 );
	lcore_rx_init_conf( &lcore, LCORE_RX );

	void *packets[ RX_BURST ];
	uint16_t rx_pkts;
	char buf[128];

	uint16_t count_of_workers = 0;
	uint16_t current_worker = 0;
	int j = 0;

	struct worker_ring_conf *wrks;
	wrks = rte_malloc( NULL, sizeof( struct worker_ring_conf ) * global_config.workers_count, 0 );

	port_init( port, mbuf_pool );

        for( int i = 0; i < global_config.workers_count; i++ )
        {
		memset( buf, 0, 128);
		snprintf( buf, 128, "worker_%d", i );

		wrks[ count_of_workers ].ring =  ring_wait_and_lookup( buf );
		wrks[ count_of_workers ].enabled = 0;
		count_of_workers++;
	}

	while( 1 )
	{
		rx_pkts = rte_eth_rx_burst( port, 0, (struct rte_mbuf **)packets, RX_BURST );
		if( unlikely( rx_pkts == 0 ) )
		{
			rx_process_messages( wrks );
			continue;
		}

		RTE_LOG( DEBUG, LCORE_RX, "Received %d packets.\n", rx_pkts );

		while( wrks[ current_worker ].enabled != 1 )
		{
			current_worker++;
			if( current_worker >= count_of_workers )
			{
				current_worker = 0;
				RTE_LOG( ALERT, LCORE_RX, "No active workers!\n" );
				rx_process_messages( wrks );
			}
		}

		int res = rte_ring_enqueue_bulk( wrks[ current_worker ].ring, (void**)packets, rx_pkts, NULL );
		if( res == -ENOBUFS )
		{
			RTE_LOG( ALERT, LCORE_RX, "Ring for worker %d is overflowed. Dropping %d packets.\n", current_worker, rx_pkts );
			for( int i = 0; i < rx_pkts; i += 1 )
			{
				rte_pktmbuf_free( packets[ i ] );
			}
		}

		current_worker++;
		if( current_worker >= count_of_workers )
		{
			current_worker = 0;
			//rx_process_messages( wrks );
			RTE_LOG( DEBUG, LCORE_RX, "Iterate over workers again\n" );
		}

		j++;
		if( unlikely( j == 10000 ) )
		{
			rx_process_messages( wrks );
		}
	}

	return 0;
}

int lcore_rx( void )
{
//	uint16_t cur_port = 0;
	uint16_t nb_ports = rte_eth_dev_count_avail();

	void *packets[ RX_BURST ];
	uint16_t rx_pkts;
	char buf[128];

	RTE_LOG( INFO, LCORE_RX, "Started on core %d for all ports\n", rte_lcore_id() );
	lcore_rx_init_conf( &lcore, LCORE_RX );
	sleep( 3 );

	uint16_t count_of_workers = 0;
	uint16_t current_worker = 0;
	int j = 0;

	struct worker_ring_conf *wrks;
	wrks = rte_malloc( NULL, sizeof( struct worker_ring_conf ) * global_config.workers_count, 0 );

	for( int i = 0; i < nb_ports; i++ )
	{
		port_init( i, mbuf_pool );
	}

	RTE_LOG( INFO, LCORE_RX, "Port is inited\n" );


        for( int i = 0; i < global_config.workers_count; i++ )
        {
		memset( buf, 0, 128);
		snprintf( buf, 128, "worker_%d", i );

		RTE_LOG( INFO, LCORE_RX, "Looking up for a worker_%d ring\n", i );

		wrks[ count_of_workers ].ring =  ring_wait_and_lookup( buf );
		wrks[ count_of_workers ].enabled = 0;
		count_of_workers++;
	}

	RTE_LOG( INFO, LCORE_RX, "Starting the loop\n" );

	while( 1 )
	{
		for( int i = 0; i < nb_ports; i++ )
		{
			rx_pkts = rte_eth_rx_burst( i, 0, (struct rte_mbuf **)packets, RX_BURST );
			if( unlikely( rx_pkts == 0 ) )
			{
				rx_process_messages( wrks );
				continue;
			}

			RTE_LOG( DEBUG, LCORE_RX, "Received %d packets.\n", rx_pkts );

			while( wrks[ current_worker ].enabled != 1 )
			{
				current_worker += 1;

				if( current_worker >= count_of_workers )
				{
					current_worker = 0;
					RTE_LOG( ALERT, LCORE_RX, "No active workers!\n" );
					rx_process_messages( wrks );
				}
			}

			int res = rte_ring_enqueue_bulk( wrks[ current_worker ].ring, (void**)packets, rx_pkts, NULL );
			if( res == 0 )
			{
				RTE_LOG( ALERT, LCORE_RX, "Ring for worker %d is overflowed. Dropping %d packets.\n", current_worker, rx_pkts );
				for( int i = 0; i < rx_pkts; i += 1 )
				{
					rte_pktmbuf_free( packets[ i ] );
				}
			}

			current_worker++;
			if( current_worker >= count_of_workers )
			{
				current_worker = 0;
				//rx_process_messages( wrks );
				RTE_LOG( DEBUG, LCORE_RX, "Iterate over workers again\n" );
			}

			if( unlikely( j++ == 100000 ) )
			{
				rx_process_messages( wrks );
			}
		}
	}

	return 0;
}

