#define RX_BURST	8
#define RX_RING		1024
#define RX_MAX_PORTS	16

#define IPC_RX_START_WORKER	0x11
#define IPC_RX_STOP_WORKER	0x12
#define IPC_RX_STARTED_WORKER	0x21
#define IPC_RX_STOPPED_WORKER	0x22

struct worker_ring_conf
{
	uint8_t enabled;
	struct rte_ring *ring;
};

void rx_process_messages( struct worker_ring_conf *wrks );
int lcore_rx( void );
int lcore_rx_per_port( void * port );
