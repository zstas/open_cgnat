#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <rte_hash.h>
#include <rte_rwlock.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_icmp.h>

#include "cgnat.h"
#include "config.h"
#include "port.h"
#include "lcore.h"
#include "lcore_tx.h"
#include "lcore_rx.h"
#include "lcore_txrx.h"
#include "worker.h"
#include "bitarray.h"

#define VERSION "v0.1"

#define RX_RING_SIZE 512
#define TX_RING_SIZE 1024

#define NUM_MBUFS       8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE      64

#define MSG_POOL_SIZE   1024
#define MSG_MAX_SIZE    1024

#define RTE_LOGTYPE_LCORE_RX    RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_LCORE_TX    RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_LCORE_TXRX  RTE_LOGTYPE_USER3
#define RTE_LOGTYPE_WORKER      RTE_LOGTYPE_USER4
#define RTE_LOGTYPE_MAIN        RTE_LOGTYPE_USER5

#define ONE_TXRX        0x1
#define MANY_TXRX       0x2
#define ONE_TX_ONE_RX   0x3
#define MANY_TX_MANY_RX 0x4

#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8
#define IPv4_BYTES(addr) \
                (uint8_t) (((addr) >> 24) & 0xFF),\
                (uint8_t) (((addr) >> 16) & 0xFF),\
                (uint8_t) (((addr) >> 8) & 0xFF),\
                (uint8_t) ((addr) & 0xFF)
#endif

struct app_config
{
        uint8_t workers_count;
        uint8_t rxs_count;

        struct lcore_conf *lcore_worker;
        struct lcore_conf *lcore_rx;

        uint8_t mode;
        uint16_t vlan_in;
        uint16_t vlan_out;
        char *eal_args[ 16 ];
        int eal_args_count;

	struct cgnat_pool_conf pool_conf;
	struct cgnat_pool pool;
};


struct ipc_msg
{
        uint8_t type;
        uint8_t data[ IPC_MSG_SIZE ];
};

void lcore_main( void );
void print_version( void );
int parse_args( int argc, char ** argv );
void print_help( void );
int schedule_lcores( uint8_t lcore_sched_scheme );
void init_ipc_to_lcores( void );
void run_worker( uint8_t id );
void stop_worker( uint8_t id );
void reload_worker( uint8_t id );
void reload_all_workers( void );
void reload_handler( int sig );

