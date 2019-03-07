#define VLAN_BOUND	0x1
#define DOT1Q_TRANSPORT	0x2
#define QINQ_TRANSPORT	0x4

#define PORT_ENABLE	0x1
#define PORT_DISABLE	0x2

#define MAX_PORTS_IN_SYSTEM	0xA

struct port_device
{
	uint8_t slot;
	uint8_t port;
	uint8_t admin_status;
	char description[64];
};

int port_init( uint8_t port, struct rte_mempool *mbuf_pool );
void port_change_state( uint8_t port, uint8_t state );
void get_port_device( uint8_t port, struct port_device *device );
