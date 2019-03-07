#include "main.h"

extern struct app_config global_config;
__thread __typeof__( struct lcore_conf ) lcore;

__thread __typeof__( uint8_t ) worker_id;

__thread __typeof__( char * ) redirect_message;
__thread __typeof__( int ) redirect_message_size;

void
worker_process_messages( void )
{
        struct ipc_msg *msg;

        if( rte_ring_dequeue( lcore.from_main, ( void ** )&msg ) != 0 )
                return;

	RTE_LOG( INFO, WORKER , "Got a ipc message with type 0x%x\n", msg->type );
	switch( msg->type )
	{
		case IPC_WORKER_RELOAD:
			RTE_LOG( INFO, WORKER, "Worker %d got msg RELOAD\n", worker_id );
			//setup_hash( global_config.registry, global_config.ssl_hosts, global_config.watchlist );
			msg->type = IPC_WORKER_RESPONSE;
			rte_ring_enqueue( lcore.to_main, ( void * )msg );
			break;
		default:
			RTE_LOG( INFO, WORKER , "Worker %d got unknown message with type 0x%x\n", worker_id, msg->type );
			rte_free( msg );
	}
}

int lcore_worker( void *w_id )
{
	worker_id = *( uint8_t* )( w_id );
	RTE_LOG( DEBUG, WORKER , "Launching worker %d...\n", worker_id );

	lcore_worker_init_conf( &lcore, LCORE_WORKER, worker_id );

	char buf[128];
	memset( buf, 0, 128);
	snprintf( buf, 128, "worker_%d", worker_id );

	struct rte_ring * ring;
	ring = rte_ring_create( buf, WORKER_RING_SIZE, rte_socket_id(), RING_F_SC_DEQ );

	uint8_t nb_ports = rte_eth_dev_count_avail();
	uint8_t port = 0;

	struct rte_ring *ring_tx[10];

	for( port = 0; port < nb_ports; port++ )
	{
		memset( buf, 0, 128);
		snprintf( buf, 128, "port_%d_tx", port );
		while( rte_ring_lookup( buf ) == NULL )
		{
			sleep(1);
			RTE_LOG( INFO, WORKER , "Waiting for ring tx port %d\n", port );
		}

                ring_tx[ port ] = rte_ring_lookup( buf );
	}

	RTE_LOG( INFO, WORKER , "All preparation for worker %d is done. Start processing\n", worker_id );

	void *packets[32];
	struct rte_mbuf *pkt;
	uint16_t rx_pkts = 32;

	void *tx_packets[32];
	uint16_t tx_pkts = 32;

	struct ether_hdr *eth_hdr;
	struct vlan_hdr *vlan_hdr;

	int32_t action;

	uint32_t j = 0;

	//FILE *fp;
	//fp = fopen("dump.txt", "w");

	while( 1 )
	{
		tx_pkts = 0;
		memset( tx_packets, 0, 32 );
		rx_pkts = rte_ring_dequeue_burst( ring, packets, 32, NULL );

		for( uint8_t i = 0; i < rx_pkts; i++ )
		{
			RTE_LOG( DEBUG, WORKER, "Worker %d got %d pkts\n", worker_id, rx_pkts );

			pkt = (struct rte_mbuf*)packets[i];
			rte_prefetch0( rte_pktmbuf_mtod( pkt, void * ) );

			eth_hdr = rte_pktmbuf_mtod( pkt, struct ether_hdr * );
			if( eth_hdr->ether_type != rte_cpu_to_be_16( ETHER_TYPE_VLAN ) )
			{
				//processed_packets[ tx_pkts++ ] = pkt;
				rte_pktmbuf_free( pkt );
				continue;
			}

			vlan_hdr = ( struct vlan_hdr * )( eth_hdr + 1 );

			if( ( rte_cpu_to_be_16( vlan_hdr->vlan_tci ) & 0xFFF ) == global_config.vlan_in )
			{
				action = cgnat_translate_inside( &global_config.pool, pkt );
				switch( action )
				{
					case ACTION_PASS_TO_WAN:
						RTE_LOG( DEBUG, WORKER, "Worker %d sending 1 pkt to WAN\n", worker_id );
						vlan_hdr->vlan_tci = rte_cpu_to_be_16( ( rte_be_to_cpu_16( vlan_hdr->vlan_tci ) & 0xF000 ) + global_config.vlan_out );
						//rte_pktmbuf_dump( fp, pkt, 128 );
						tx_packets[ tx_pkts++ ] = pkt;
						//rte_ring_enqueue( ring_tx[ pkt->port ], pkt );
						continue;
						break;
					case ACTION_DROP:
						RTE_LOG( DEBUG, WORKER, "Worker %d dropping 1 pkt\n", worker_id );
						rte_pktmbuf_free( pkt );
						continue;
						break;
				}
				vlan_hdr->vlan_tci = rte_cpu_to_be_16( ( rte_be_to_cpu_16( vlan_hdr->vlan_tci ) & 0xF000 ) + global_config.vlan_out );
				tx_packets[ tx_pkts++ ] = pkt;
				//rte_ring_enqueue( ring_tx[ pkt->port ], pkt );
			}
			else if( ( rte_cpu_to_be_16( vlan_hdr->vlan_tci ) & 0xFFF ) == global_config.vlan_out )
			{
				action = cgnat_translate_outside( &global_config.pool, pkt );
				switch( action )
				{
					case ACTION_PASS_TO_LAN:
						RTE_LOG( DEBUG, WORKER, "Worker %d sending 1 pkt to LAN\n", worker_id );
						vlan_hdr->vlan_tci = rte_cpu_to_be_16( ( rte_be_to_cpu_16( vlan_hdr->vlan_tci ) & 0xF000 ) + global_config.vlan_in );
						tx_packets[ tx_pkts++ ] = pkt;
						//rte_ring_enqueue( ring_tx[ pkt->port ], pkt );
						continue;
						break;
					case ACTION_DROP:
						RTE_LOG( DEBUG, WORKER, "Worker %d dropping 1 pkt\n", worker_id );
						rte_pktmbuf_free( pkt );
						continue;
						break;
				}
				vlan_hdr->vlan_tci = rte_cpu_to_be_16( ( rte_be_to_cpu_16( vlan_hdr->vlan_tci ) & 0xF000 ) + global_config.vlan_in );
				tx_packets[ tx_pkts++ ] = pkt;
				//rte_ring_enqueue( ring_tx[ pkt->port ], pkt );
			}
			else
			{
				//processed_packets[ tx_pkts++ ] = pkt;
				rte_pktmbuf_free( pkt );
				continue;
			}
		}
		rte_ring_enqueue_bulk( ring_tx[ 1 ], tx_packets, tx_pkts, NULL );

		if( unlikely( j++ == 10000 ) )
		{
			worker_process_messages();
			j = 0;
		}
	}

	return 0;
}
