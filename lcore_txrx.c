#include "main.h"

int lcore_txrx( void * port )
{
	RTE_LOG( DEBUG, LCORE_TXRX, "Starting lcore txrx...\n" );
	if( port == NULL )
		RTE_LOG( INFO, LCORE_TXRX, "started on core %d for all ports\n", rte_lcore_id() );
	else
		RTE_LOG( INFO, LCORE_TXRX, "started on core %d for port %u\n", rte_lcore_id(), *(uint16_t *)port );

	while( 1 )
	{
	}

	return 0;
}
