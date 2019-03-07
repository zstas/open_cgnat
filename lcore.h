#define LCORE_MAIN      0x1
#define LCORE_WORKER    0x2
#define LCORE_RX        0x3
#define LCORE_TX        0x4
#define LCORE_TXRX      0x5

#define MAX_CORES       64

struct lcore_conf
{
        uint8_t id;
        uint8_t type;
        struct rte_ring *from_main;
        struct rte_ring *to_main;
};

void lcore_worker_init_conf( struct lcore_conf *lcore, uint8_t type, uint8_t id );
void lcore_rx_init_conf( struct lcore_conf *lcore, uint8_t type );
struct rte_ring * ring_wait_and_lookup( char * buf );

