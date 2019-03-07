#include "main.h"

static const struct rte_eth_conf port_conf_default =
{
        .rxmode =
        {
                .max_rx_pkt_len = ETHER_MAX_LEN,
		.offloads = DEV_RX_OFFLOAD_CHECKSUM
//                .hw_ip_checksum = 1,
//                .hw_strip_crc   = 1
        },
	.txmode =
	{
		.offloads = DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM
	}
};

int
port_init( uint8_t port, struct rte_mempool *mbuf_pool )
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if( port >= rte_eth_dev_count_avail() )
		return -1;

	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get( port, &dev_info );

	//RTE_LOG( INFO, MAIN, "Port %d offload_capa %lx\n", port, dev_info.tx_offload_capa );
	//port_conf.txmode.offloads = dev_info.tx_offload_capa;

	//struct rte_eth_txconf txconf;
	//txconf.offloads = port_conf.txmode.offloads;
	//txconf.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM;

	retval = rte_eth_dev_configure( port, rx_rings, tx_rings, &port_conf );
	if( retval != 0 )
		return retval;

	for( q = 0; q < rx_rings; q++ )
	{
		retval = rte_eth_rx_queue_setup( port, q, RX_RING_SIZE, rte_eth_dev_socket_id( port ), NULL, mbuf_pool );
		if( retval < 0 )
			return retval;
	}

	for( q = 0; q < tx_rings; q++ )
	{
		retval = rte_eth_tx_queue_setup( port, q, TX_RING_SIZE, rte_eth_dev_socket_id( port ), 0 );
		if( retval < 0 )
			return retval;
	}

	retval = rte_eth_dev_start( port );
	if( retval < 0 )
		return retval;

	rte_eth_promiscuous_enable( port );

	return 0;
}

void
port_change_state( uint8_t port, uint8_t state )
{
	switch( state )
	{
		case PORT_ENABLE:
			rte_eth_dev_set_link_up( port );
			break;
		case PORT_DISABLE:
			rte_eth_dev_set_link_down( port );
			break;
		default:
			break;
	}
}

void get_port_device( uint8_t port, struct port_device *device )
{
	char buf[256];
	rte_eth_dev_get_name_by_port( port, buf );
	uint8_t l_domain, l_bus, l_device, l_function;
	sscanf( buf, "%"SCNu8":%"SCNu8":%"SCNu8".%"SCNu8, &l_domain, &l_bus, &l_device, &l_function );
	device->slot = l_bus;
	device->port = l_function;
	RTE_LOG( INFO, MAIN, "Slot/Port is %u/%u\n", l_bus, l_function );
}
