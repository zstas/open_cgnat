#define TX_BURST	64
#define TX_RING		2048
#define TX_MAX_PORTS	16

int lcore_tx_per_port( void * port );
int lcore_tx( void * port );
