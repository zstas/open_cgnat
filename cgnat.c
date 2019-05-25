#include "main.h"

extern rte_atomic64_t timestamp;

int cgnat_init_pool( struct cgnat_pool_conf *conf, struct cgnat_pool *pool, uint8_t id )
{
	RTE_LOG( INFO, MAIN, "Initializating pool_%d with pba_size %d\n", id, 1 << conf->pba_size );
	pool->conf = *conf;
	pool->index = id;
	pool->pba_count = ( conf->port_to - conf->port_from ) / ( 1 << conf->pba_size );
	RTE_LOG( INFO, MAIN, " pba_count: %d\n", pool->pba_count );

	pool->xlations = rte_calloc( NULL, 1 << conf->maximum_xlations, sizeof( struct cgnat_translation ), 0 );
	RTE_LOG( INFO, MAIN, "Creating array for xlations with count of members %d\n", 1 << conf->maximum_xlations );
	if( pool->xlations == NULL ) return NOT_ENOUGH_MEMORY_TO_INIT;

	pool->subscribers = rte_calloc( NULL, 1 << conf->maximum_subscribers, sizeof( struct cgnat_subscriber ), 0 );
	RTE_LOG( INFO, MAIN, "Creating array for subscribers with count of members %d\n", 1 << conf->maximum_subscribers );
	if( pool->subscribers == NULL ) return NOT_ENOUGH_MEMORY_TO_INIT;

	char s[64];
	snprintf( s, sizeof( s ), "sub_to_pub_%d", pool->index );

	struct rte_hash_parameters hash_params =
	{
		.name = s,
		.entries = 1 << conf->maximum_xlations,
		.hash_func_init_val = 0,
		.key_len = sizeof( struct five_tuple ),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF,
	};

	pool->sub_to_pub = rte_hash_create( &hash_params );
	RTE_LOG( INFO, MAIN, "Creating hash table %s with address %p\n", s, pool->sub_to_pub );
	if( pool->sub_to_pub == NULL ) return NOT_ENOUGH_MEMORY_TO_INIT;

	snprintf( s, sizeof( s ), "pub_to_sub_%d", pool->index );
	pool->pub_to_sub = rte_hash_create( &hash_params );
	RTE_LOG( INFO, MAIN, "Creating hash table %s with address %p\n", s, pool->pub_to_sub );
	if( pool->pub_to_sub == NULL ) return NOT_ENOUGH_MEMORY_TO_INIT;

	snprintf( s, sizeof( s ), "subscribers_%d", pool->index );
	hash_params.entries = 1 << conf->maximum_subscribers;
	hash_params.key_len = sizeof( uint32_t );

	pool->subscribers_hash = rte_hash_create( &hash_params );
	RTE_LOG( INFO, MAIN, "Creating hash table %s with address %p\n", s, pool->subscribers_hash );
	if( pool->subscribers_hash == NULL ) return NOT_ENOUGH_MEMORY_TO_INIT;

	pool->last_paired_address = conf->ip_from;
	pool->addresses = rte_calloc( NULL, ( conf->ip_to - conf->ip_from ), sizeof( struct cgnat_pool_entry ), 0 );
	RTE_LOG( INFO, MAIN, "Creating addresses array with address %p\n", pool->addresses );

	for( uint32_t i = 0; i <= ( conf->ip_to - conf->ip_from ); i++ )
	{
		cgnat_init_pool_entry( pool, &pool->addresses[ i ], conf->ip_from + i );
	}

	return 0;
}

void cgnat_init_pool_entry( struct cgnat_pool *pool, struct cgnat_pool_entry *entry, uint32_t ip_address )
{
	RTE_LOG( INFO, MAIN, "Initialization cgnat_init_pool_entry with ip " IPv4_BYTES_FMT " and pba_size: %d\n", IPv4_BYTES( ip_address ), pool->pba_count );
	entry->ip_address = ip_address;
	entry->portblocks = rte_calloc( NULL, pool->pba_count, sizeof( struct cgnat_pba_entry ), 0 );
	uint8_t bytes = ( 1 << pool->conf.pba_size ) / 8;
	for( int i = 0; i < pool->pba_count; i++ )
	{
		entry->portblocks[i].tcp = rte_calloc( NULL, bytes, sizeof( struct bitmask_ports ), 0 );
		entry->portblocks[i].udp = rte_calloc( NULL, bytes, sizeof( struct bitmask_ports ), 0 );
		entry->portblocks[i].icmp = rte_calloc( NULL, bytes, sizeof( struct bitmask_ports ), 0 );
	}
}

void cgnat_five_tuple_init( struct five_tuple *t, uint32_t src_address, uint16_t src_port, uint8_t proto, uint32_t dst_address, uint16_t dst_port )
{
	t->src_address = src_address;
	t->dst_address = dst_address;
	t->src_port = src_port;
	t->dst_port = dst_port;
	t->proto = proto;
}

uint32_t cgnat_get_paired_ip( struct cgnat_pool *pool, uint32_t subscriber )
{
	int32_t subscriber_index = rte_hash_lookup( pool->subscribers_hash, &subscriber );
	if( subscriber_index >= 0 ) //Subscriber found
	{
		RTE_LOG( DEBUG, MAIN, "Found existing paired_ip " IPv4_BYTES_FMT " for subscriber " IPv4_BYTES_FMT "\n", IPv4_BYTES( pool->subscribers[ subscriber_index ].paired_ip ), IPv4_BYTES( subscriber ) );
		return pool->subscribers[ subscriber_index ].paired_ip;
	}

	uint32_t paired_ip = pool->last_paired_address;
	RTE_LOG( DEBUG, MAIN, "Allocated new paired_ip " IPv4_BYTES_FMT " for subscriber " IPv4_BYTES_FMT "\n", IPv4_BYTES( paired_ip ), IPv4_BYTES( subscriber ) );

	subscriber_index = rte_hash_add_key( pool->subscribers_hash, &subscriber );
	pool->subscribers[ subscriber_index ].subscriber_ip = subscriber;
	pool->subscribers[ subscriber_index ].paired_ip = paired_ip;
	pool->subscribers[ subscriber_index ].tcp_xlations = 0;
	pool->subscribers[ subscriber_index ].udp_xlations = 0;
	pool->subscribers[ subscriber_index ].icmp_xlations = 0;

	pool->last_paired_address++;
	if( pool->last_paired_address == pool->conf.ip_to )
		pool->last_paired_address = pool->conf.ip_from;

	return paired_ip;
}

static inline int8_t cgnat_alloc_pba( struct cgnat_pool *pool, struct cgnat_pool_entry *entry, uint32_t subscriber )
{
	for( int i = 0; i < pool->pba_count; i++ )
	{
		if( entry->portblocks[ i ].subscriber == 0 )
		{
			entry->portblocks[ i ].subscriber = subscriber;
			RTE_LOG( DEBUG, MAIN, "Allocated pba %d for subscriber " IPv4_BYTES_FMT "\n", i, IPv4_BYTES( subscriber ) );
			return 0;
		}
	}
	return -1;
}

static inline int8_t cgnat_alloc_port( struct cgnat_pool *pool, struct cgnat_pool_entry *entry, uint32_t subscriber, uint8_t proto, uint16_t *selected_port )
{
	RTE_LOG( DEBUG, MAIN, "Allocation port for subscriber " IPv4_BYTES_FMT "\n", IPv4_BYTES( subscriber ) );
	for( int i = 0; i < pool->pba_count; i++ )
	{
		if( entry->portblocks[ i ].subscriber != subscriber )
			continue;

		for( int j = 0; j < ( ( 1 << pool->conf.pba_size ) / 8 ); j++ )
		{
			RTE_LOG( DEBUG, MAIN, "Iterating over pba i=%d j=%d\n", i, j );
			struct bitmask_ports *bmp = NULL;
			switch( proto )
			{
				case IPPROTO_TCP:
					bmp = &entry->portblocks[ i ].tcp[ j ];
					break;
				case IPPROTO_UDP:
					bmp = &entry->portblocks[ i ].udp[ j ];
					break;
				case IPPROTO_ICMP:
					bmp = &entry->portblocks[ i ].icmp[ j ];
					break;
				default:
					break;
			}
			if( bmp == NULL ) 
			{
				RTE_LOG( DEBUG, MAIN, "Cannot find right protocol for cgnat_alloc_port\n" );
				return -1;
			}
			if( bmp->port0 == 0 ) { bmp->port0 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8; return 0; }
			if( bmp->port1 == 0 ) { bmp->port1 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 1; return 0; }
			if( bmp->port2 == 0 ) { bmp->port2 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 2; return 0; }
			if( bmp->port3 == 0 ) { bmp->port3 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 3; return 0; }
			if( bmp->port4 == 0 ) { bmp->port4 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 4; return 0; }
			if( bmp->port5 == 0 ) { bmp->port5 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 5; return 0; }
			if( bmp->port6 == 0 ) { bmp->port6 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 6; return 0; }
			if( bmp->port7 == 0 ) { bmp->port7 = 1; *selected_port = pool->conf.port_from + i * ( 1 << pool->conf.pba_size ) + j * 8 + 7; return 0; }
		}
	}
	RTE_LOG( DEBUG, MAIN, "Not found any pba with free entries for subscriber " IPv4_BYTES_FMT "\n", IPv4_BYTES( subscriber ) );
	return -1;
}

static inline void cgnat_dealloc_port( struct cgnat_pool *pool, uint32_t subscriber, uint8_t proto, uint16_t port )
{
}

uint32_t cgnat_allocate_inside_translation( struct cgnat_pool *pool, struct five_tuple tuple )
{
	uint32_t paired_ip = cgnat_get_paired_ip( pool, tuple.src_address );
	uint16_t selected_port = 0;
	struct cgnat_pool_entry *entry = &pool->addresses[ paired_ip - pool->conf.ip_from ];
	RTE_LOG( DEBUG, MAIN, "Choosing pool entry with ip " IPv4_BYTES_FMT "\n", IPv4_BYTES( entry->ip_address ) );

	if( cgnat_alloc_port( pool, entry, tuple.src_address, tuple.proto, &selected_port ) == -1 )
	{
		cgnat_alloc_pba( pool, entry, tuple.src_address );
		if( cgnat_alloc_port( pool, entry, tuple.src_address, tuple.proto, &selected_port ) == -1 )
			return 0;
	}

	struct five_tuple global_tuple;
	cgnat_five_tuple_init( &global_tuple, tuple.dst_address, tuple.dst_port, tuple.proto, paired_ip, selected_port );

	int64_t xl = rte_hash_add_key( pool->sub_to_pub, &tuple );
	RTE_LOG( DEBUG, MAIN, "Adding xlation key to hash_table with result %ld\n", xl );
	if( xl < 0 )
	{
		RTE_LOG( DEBUG, MAIN, "Cannot add xlation to hash_table!\n" );
		return 0;
	}

	rte_hash_add_key_data( pool->pub_to_sub, &global_tuple, &xl );

	pool->xlations[ xl ].private_ip = tuple.src_address;
	pool->xlations[ xl ].private_port = tuple.src_port;
	pool->xlations[ xl ].proto = tuple.proto;
	pool->xlations[ xl ].public_ip = paired_ip;
	pool->xlations[ xl ].public_port = selected_port;
	pool->xlations[ xl ].global_ip = tuple.dst_address;
	pool->xlations[ xl ].global_port = tuple.dst_port;

	int32_t subscriber_id = rte_hash_lookup( pool->subscribers_hash, ( void * )&tuple.src_address );
	switch( tuple.proto )
	{
		case IPPROTO_TCP:
			pool->subscribers[ subscriber_id ].tcp_xlations++;
			break;
		case IPPROTO_UDP:
			pool->subscribers[ subscriber_id ].udp_xlations++;
			break;
		case IPPROTO_ICMP:
			pool->subscribers[ subscriber_id ].icmp_xlations++;
			break;
	}

	RTE_LOG( DEBUG, MAIN, "Allocated xlation " IPv4_BYTES_FMT ":%d " IPv4_BYTES_FMT ":%d proto %d\n", IPv4_BYTES( tuple.src_address ), tuple.src_port, IPv4_BYTES( paired_ip ), selected_port, tuple.proto );
	return xl;
}

void cgnat_print_xlation( struct cgnat_pool *pool, uint32_t index )
{
	RTE_LOG( DEBUG, WORKER, "Dumped xlation " IPv4_BYTES_FMT ":%d " IPv4_BYTES_FMT ":%d " IPv4_BYTES_FMT ":%d proto %d updated_ad:%ld\n",
		 IPv4_BYTES( pool->xlations[ index ].private_ip ), pool->xlations[ index ].private_port,
		 IPv4_BYTES( pool->xlations[ index ].public_ip ), pool->xlations[ index ].public_port,
		 IPv4_BYTES( pool->xlations[ index ].global_ip ), pool->xlations[ index ].global_port,
		 pool->xlations[ index ].proto, pool->xlations[ index ].updated_at );
}

void cgnat_icmp_checksum( struct icmp_hdr *icmp, uint16_t len )
{
	uint32_t sum = 0;
	icmp->icmp_cksum = 0;
	uint16_t *addr = ( uint16_t * )icmp;

	while( len > 1 )
	{
		sum += *( addr++ );
		RTE_LOG( DEBUG, WORKER, "Sum is: %x\n", sum );
		len -= 2;
	}

	if( len > 0 )
		sum += *( uint8_t * )addr;

	RTE_LOG( DEBUG, WORKER, "Sum is: %x\n", sum );

	while( sum >> 16 )
		sum = ( sum & 0xFFFF ) + ( sum >> 16 );

	RTE_LOG( DEBUG, WORKER, "Collapsed Sum is: %x\n", sum );

	icmp->icmp_cksum = ~sum;
}

int32_t cgnat_translate_inside( struct cgnat_pool *pool, struct rte_mbuf *pkt )
{
	struct five_tuple inside_to_outside;
	struct tcp_hdr *tcp;
	struct udp_hdr *udp;
	struct icmp_hdr *icmp;
	int32_t ret = 0;

	rte_prefetch0( rte_pktmbuf_mtod( pkt, void * ) );

	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod( pkt, struct ether_hdr * );

	if( eth_hdr->ether_type != rte_cpu_to_be_16( ETHER_TYPE_VLAN ) )
		return ACTION_DROP;

	pkt->l2_len = sizeof( struct ether_hdr ) + 4;

	struct vlan_hdr *vlan_hdr = ( struct vlan_hdr * )( eth_hdr + 1 );
	if( vlan_hdr->eth_proto != rte_cpu_to_be_16( ETHER_TYPE_IPv4 ) )
		return ACTION_PASS_TO_WAN;

	struct ipv4_hdr *ipv4 = rte_pktmbuf_mtod_offset( pkt, struct ipv4_hdr *, sizeof(struct ether_hdr) + 4 );
	pkt->l3_len = sizeof( struct ipv4_hdr );
	pkt->ol_flags = PKT_TX_IP_CKSUM;

	switch( ipv4->next_proto_id )
	{
		case IPPROTO_TCP:
			tcp = ( struct tcp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &inside_to_outside, rte_be_to_cpu_32( ipv4->src_addr ), rte_be_to_cpu_16( tcp->src_port ), IPPROTO_TCP, rte_be_to_cpu_32( ipv4->dst_addr ), rte_be_to_cpu_16( tcp->dst_port ) );

			ret = rte_hash_lookup( pool->sub_to_pub, &inside_to_outside );
			if( ret < 0 )
			{
				ret = cgnat_allocate_inside_translation( pool, inside_to_outside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			//cgnat_print_xlation( pool, ret );
			ipv4->src_addr = rte_cpu_to_be_32( pool->xlations[ ret ].public_ip );
			tcp->src_port = rte_cpu_to_be_16( pool->xlations[ ret ].public_port );
			pool->xlations[ ret ].counters.sub_to_pub_pkts++;
			//pool->xlations[ ret ].updated_at = time( NULL );
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );
			//pool->xlations[ ret ].counters.sub_to_pub_bytes += len;

			pkt->l4_len = sizeof( struct tcp_hdr );
			pkt->ol_flags |= PKT_TX_TCP_CKSUM;
			ipv4->hdr_checksum = 0;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );

			return ACTION_PASS_TO_WAN;
			break;
		case IPPROTO_UDP:
			udp = ( struct udp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &inside_to_outside, rte_be_to_cpu_32( ipv4->src_addr ), rte_be_to_cpu_16( udp->src_port ), IPPROTO_TCP, rte_be_to_cpu_32( ipv4->dst_addr ), rte_be_to_cpu_16( udp->dst_port ) );

			ret = rte_hash_lookup( pool->sub_to_pub, &inside_to_outside );
			if( ret < 0 )
			{
				ret = cgnat_allocate_inside_translation( pool, inside_to_outside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			//cgnat_print_xlation( pool, ret );
			ipv4->src_addr = rte_cpu_to_be_32( pool->xlations[ ret ].public_ip );
			udp->src_port = rte_cpu_to_be_16( pool->xlations[ ret ].public_port );
			pool->xlations[ ret ].counters.sub_to_pub_pkts++;
			//pool->xlations[ ret ].updated_at = time( NULL );
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );
			//pool->xlations[ ret ].counters.sub_to_pub_bytes += len;

			pkt->l4_len = sizeof( struct tcp_hdr );
			pkt->ol_flags |= PKT_TX_UDP_CKSUM;
			ipv4->hdr_checksum = 0;
			udp->dgram_cksum = 0;
			udp->dgram_cksum = rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );

			return ACTION_PASS_TO_WAN;
			break;
		case IPPROTO_ICMP:
			icmp = ( struct icmp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &inside_to_outside, rte_be_to_cpu_32( ipv4->src_addr ), rte_be_to_cpu_16( icmp->icmp_ident ), IPPROTO_ICMP, rte_be_to_cpu_32( ipv4->dst_addr ), 0 );

			ret = rte_hash_lookup( pool->sub_to_pub, &inside_to_outside );
			if( ret < 0 )
			{
				ret = cgnat_allocate_inside_translation( pool, inside_to_outside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			//cgnat_print_xlation( pool, ret );
			ipv4->src_addr = rte_cpu_to_be_32( pool->xlations[ ret ].public_ip );
			icmp->icmp_ident = rte_cpu_to_be_16( pool->xlations[ ret ].public_port );
			pool->xlations[ ret ].counters.sub_to_pub_pkts++;
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );
			//pool->xlations[ ret ].updated_at = time( NULL );

			cgnat_icmp_checksum( icmp, rte_be_to_cpu_16( ipv4->total_length ) - sizeof( struct ipv4_hdr ) );
			ipv4->hdr_checksum = 0;
			//rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );
			//ipv4->hdr_checksum = rte_ipv4_cksum( ipv4 );

			return ACTION_PASS_TO_WAN;
			break;

	}
	return ACTION_PASS_TO_WAN;
}


int32_t cgnat_translate_outside( struct cgnat_pool *pool, struct rte_mbuf *pkt )
{
	int32_t ret = 0;
	struct tcp_hdr *tcp;
	struct udp_hdr *udp;
	struct icmp_hdr *icmp;
	struct five_tuple outside_to_inside;

	rte_prefetch0( rte_pktmbuf_mtod( pkt, void * ) );

	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod( pkt, struct ether_hdr * );

	if( eth_hdr->ether_type != rte_cpu_to_be_16( ETHER_TYPE_VLAN ) )
		return ACTION_DROP;

	pkt->l2_len = sizeof( struct ether_hdr ) + 4;

	struct vlan_hdr *vlan_hdr = ( struct vlan_hdr * )( eth_hdr + 1 );
	if( vlan_hdr->eth_proto != rte_cpu_to_be_16( ETHER_TYPE_IPv4 ) )
		return ACTION_PASS_TO_WAN;

	struct ipv4_hdr *ipv4 = rte_pktmbuf_mtod_offset( pkt, struct ipv4_hdr *, sizeof(struct ether_hdr) + 4 );
	pkt->l3_len = sizeof( struct ipv4_hdr );
	pkt->ol_flags = PKT_TX_IP_CKSUM;

	switch( ipv4->next_proto_id )
	{
		case IPPROTO_TCP:
			tcp = ( struct tcp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &outside_to_inside, rte_be_to_cpu_32( ipv4->src_addr ), rte_be_to_cpu_16( tcp->src_port ), IPPROTO_TCP, rte_be_to_cpu_32( ipv4->dst_addr ), rte_be_to_cpu_16( tcp->dst_port ) );

			ret = rte_hash_lookup( pool->pub_to_sub, &outside_to_inside );
			if( ret < 0 )
			{
				//ret = cgnat_allocate_inside_translation( pool, inside_to_outside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			//cgnat_print_xlation( pool, ret );
			ipv4->dst_addr = rte_cpu_to_be_32( pool->xlations[ ret ].private_ip );
			tcp->dst_port = rte_cpu_to_be_16( pool->xlations[ ret ].private_port );
			pool->xlations[ ret ].counters.sub_to_pub_pkts++;
			//pool->xlations[ ret ].updated_at = time( NULL );
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );
			//pool->xlations[ ret ].counters.sub_to_pub_bytes += len;
			ipv4->hdr_checksum = 0;
			//ipv4->hdr_checksum = rte_ipv4_cksum( ipv4 );

			pkt->l4_len = sizeof( struct tcp_hdr );
			pkt->ol_flags |= PKT_TX_TCP_CKSUM;
			ipv4->hdr_checksum = 0;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );

			return ACTION_PASS_TO_LAN;
			break;
		case IPPROTO_UDP:
			udp = ( struct udp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &outside_to_inside, rte_be_to_cpu_32( ipv4->src_addr ), rte_be_to_cpu_16( udp->src_port ), IPPROTO_TCP, rte_be_to_cpu_32( ipv4->dst_addr ), rte_be_to_cpu_16( udp->dst_port ) );

			ret = rte_hash_lookup( pool->pub_to_sub, &outside_to_inside );
			if( ret < 0 )
			{
				//ret = cgnat_allocate_inside_translation( pool, inside_to_outside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			//cgnat_print_xlation( pool, ret );
			ipv4->dst_addr = rte_cpu_to_be_32( pool->xlations[ ret ].private_ip );
			udp->dst_port = rte_cpu_to_be_16( pool->xlations[ ret ].private_port );
			pool->xlations[ ret ].counters.sub_to_pub_pkts++;
			//pool->xlations[ ret ].updated_at = time( NULL );
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );
			//pool->xlations[ ret ].counters.sub_to_pub_bytes += len;
			ipv4->hdr_checksum = 0;
			//ipv4->hdr_checksum = rte_ipv4_cksum( ipv4 );

			pkt->l4_len = sizeof( struct tcp_hdr );
			pkt->ol_flags |= PKT_TX_UDP_CKSUM;
			ipv4->hdr_checksum = 0;
			udp->dgram_cksum = 0;
			udp->dgram_cksum = rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );

			return ACTION_PASS_TO_LAN;
			break;
		case IPPROTO_ICMP:
			icmp = ( struct icmp_hdr * )( ( char *)ipv4 + ( ( ipv4->version_ihl & 0xF ) << 2 ) );
			cgnat_five_tuple_init( &outside_to_inside, rte_be_to_cpu_32( ipv4->src_addr ), 0, IPPROTO_ICMP, rte_be_to_cpu_32( ipv4->dst_addr ), rte_be_to_cpu_16( icmp->icmp_ident ) );

			ret = rte_hash_lookup( pool->pub_to_sub, &outside_to_inside );
			if( ret < 0 )
			{
				//ret = cgnat_allocate_inside_translation( pool, outside_to_inside );
			}

			if( ret < 0 )
				return ACTION_DROP;

			cgnat_print_xlation( pool, ret );
			ipv4->dst_addr = rte_cpu_to_be_32( pool->xlations[ ret ].private_ip );
			icmp->icmp_ident = rte_cpu_to_be_16( pool->xlations[ ret ].private_port );
			pool->xlations[ ret ].counters.pub_to_sub_pkts++;
			//pool->xlations[ ret ].updated_at = time( NULL );
			pool->xlations[ ret ].updated_at = rte_atomic64_read( &timestamp );

			cgnat_icmp_checksum( icmp, rte_be_to_cpu_16( ipv4->total_length ) - sizeof( struct ipv4_hdr ) );
			ipv4->hdr_checksum = 0;
			//rte_ipv4_phdr_cksum( ipv4, pkt->ol_flags );
			//ipv4->hdr_checksum = rte_ipv4_cksum( ipv4 );

			return ACTION_PASS_TO_LAN;
			break;
	}
	return ACTION_PASS_TO_LAN;
}

void cgnat_clear_expired_xlations( struct cgnat_pool *pool )
{
	for( int i = 0; i < ( 1 << pool->conf.maximum_xlations ); i++ )
	{
		if( pool->xlations[ i ].private_ip == 0 )
			continue;

		switch( pool->xlations[ i ].flags )
		{
			case CGNAT_XLATE_TCP_SYN:
				break;
			case CGNAT_XLATE_TCP_EST:
				break;
			case CGNAT_XLATE_TCP_FIN_RES:
				break;
			case CGNAT_XLATE_UDP:
				break;
			case CGNAT_XLATE_ICMP:
				break;
		}
	}
}
