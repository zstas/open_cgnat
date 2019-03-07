#include <rte_rwlock.h>

#define NOT_ENOUGH_MEMORY_TO_INIT -1

#define ACTION_DROP		0x1
#define ACTION_PASS_TO_WAN	0x2
#define ACTION_PASS_TO_LAN	0x3

#define CGNAT_XLATE_TCP_SYN	0x1
#define CGNAT_XLATE_TCP_EST	0x2
#define CGNAT_XLATE_TCP_FIN_RES 0x3
#define CGNAT_XLATE_UDP		0x4
#define CGNAT_XLATE_ICMP	0x5

#define CGNAT_XLATE_TIMEOUT_TCP_SYN	128
#define CGNAT_XLATE_TIMEOUT_TCP_EST	600
#define CGNAT_XLATE_TIMEOUT_TCP_FIN_RES	240
#define CGNAT_XLATE_TIMEOUT_TCP_UDP	120
#define CGNAT_XLATE_TIMEOUT_TCP_ICMP	60

struct cgnat_pool_conf
{
	//CGNAT pool range and pba size
	uint32_t ip_from;
	uint32_t ip_to;
	uint16_t port_from;
	uint16_t port_to;
	uint8_t pba_size;

	//Global limitations
	uint8_t maximum_subscribers;
	uint8_t maximum_xlations;

	//Limitations for subscriber
	uint16_t tcp_xlations_per_sub;
	uint16_t udp_xlations_per_sub;
	uint16_t icmp_xlations_per_sub;

	//Timeouts
	uint16_t timeout_tcp_syn;
	uint16_t timeout_tcp_est;
	uint16_t timeout_tcp_fin_res;
	uint16_t timeout_tcp_udp;
	uint16_t timeout_tcp_icmp;

	//Flags
	uint8_t address_selection_method: 4;
	uint8_t port_block_allocation: 1;
	uint8_t endpoint_independent_mapping: 1;
	uint8_t endpoint_independent_filtering: 1;
};

struct cgnat_pool
{
	//Must be unique per app
	uint8_t index;
	struct cgnat_pool_conf conf;

	uint16_t pba_count;

	struct rte_hash *sub_to_pub;
	struct rte_hash *pub_to_sub;
	struct cgnat_translation *xlations;

	struct rte_hash *subscribers_hash;
	struct cgnat_subscriber *subscribers;

	struct cgnat_pool_entry *addresses;

	uint32_t last_paired_address;

};

struct cgnat_pool_entry
{
	uint32_t ip_address;
	struct cgnat_pba_entry *portblocks;
};

struct bitmask_ports
{
	uint8_t port0:1;
	uint8_t port1:1;
	uint8_t port2:1;
	uint8_t port3:1;
	uint8_t port4:1;
	uint8_t port5:1;
	uint8_t port6:1;
	uint8_t port7:1;
};

struct cgnat_pba_entry
{
	uint32_t subscriber;
	struct bitmask_ports *tcp;
	struct bitmask_ports *udp;
	struct bitmask_ports *icmp;
};

struct five_tuple
{
	uint32_t src_address;
	uint32_t dst_address;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
};

struct three_tuple
{
	uint32_t address;
	uint16_t port;
	uint8_t proto;
};

struct cgnat_subscriber
{
	uint32_t subscriber_ip;
	uint32_t paired_ip;

	uint16_t tcp_xlations;
	uint16_t udp_xlations;
	uint16_t icmp_xlations;
};

struct cgnat_counters
{
	uint64_t sub_to_pub_pkts;
	uint64_t sub_to_pub_bytes;
	uint64_t pub_to_sub_pkts;
	uint64_t pub_to_sub_bytes;

	uint64_t created_at;
	uint64_t updated_at;
};

struct cgnat_translation
{
	uint32_t private_ip;
	uint16_t private_port;
	uint32_t public_ip;
	uint16_t public_port;
	uint32_t global_ip;
	uint16_t global_port;
	uint8_t proto;

	uint8_t flags;
	uint64_t updated_at;

	struct cgnat_counters counters;
};

int cgnat_init_pool( struct cgnat_pool_conf *conf, struct cgnat_pool *pool, uint8_t id );
void cgnat_init_pool_entry( struct cgnat_pool *pool, struct cgnat_pool_entry *entry, uint32_t ip_address );
int32_t cgnat_subscriber_translate( struct cgnat_pool *pool, uint32_t address, uint16_t port, uint8_t proto );
int32_t cgnat_public_translate( struct cgnat_pool *pool, uint32_t address, uint16_t port, uint8_t proto );
uint32_t cgnat_iterate_paired_ip( struct cgnat_pool *pool );
uint32_t cgnat_allocate_inside_translation( struct cgnat_pool *pool, struct five_tuple tuple );
uint32_t cgnat_get_paired_ip( struct cgnat_pool *pool, uint32_t subscriber );
void cgnat_print_xlation( struct cgnat_pool *pool, uint32_t index );
int32_t cgnat_translate_inside( struct cgnat_pool *pool, struct rte_mbuf *m );
int32_t cgnat_translate_outside( struct cgnat_pool *pool, struct rte_mbuf *m );
void cgnat_five_tuple_init( struct five_tuple *t, uint32_t src_address, uint16_t src_port, uint8_t proto, uint32_t dst_address, uint16_t dst_port );
void cgnat_icmp_checksum( struct icmp_hdr *icmp, uint16_t len );
void cgnat_clear_expired_xlations( struct cgnat_pool *pool );
