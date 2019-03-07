#define DROP 			0x0
#define PASS			0x1
#define REDIRECT		0x2
#define RESET			0x3
#define RESET_IP		0x4
#define TAP_REDIRECT		0x5
#define REDIRECT_V6		0x6
#define RESET_V6		0x7

#define WORKER_RING_SIZE	8192
#define WORKER_BURST		32

#define UNCLASSIFIED			0
#define CLASS_HTTP_GET			100
#define CLASS_HTTP_GET_REASSEMBLE	101
#define CLASS_HTTPS_CLIENT_HELLO	200
#define CLASS_HTTPS_CLIENT_HELLO_SNI	201
#define CLASS_HTTPS_REASSEMBLE		202

#define IPC_RING_SIZE           512
#define IPC_MSG_SIZE            1000

#define IPC_WORKER_RELOAD       0xF3
#define IPC_WORKER_RESPONSE     0xF8

void worker_process_messages( void );
int lcore_worker( void *w_id );

#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8
#define IPv4_BYTES(addr) \
                (uint8_t) (((addr) >> 24) & 0xFF),\
                (uint8_t) (((addr) >> 16) & 0xFF),\
                (uint8_t) (((addr) >> 8) & 0xFF),\
                (uint8_t) ((addr) & 0xFF)
#endif
#ifndef IPv6_BYTES
#define IPv6_BYTES_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"\
                       "%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IPv6_BYTES(addr) \
        addr[0],  addr[1], addr[2],  addr[3], \
        addr[4],  addr[5], addr[6],  addr[7], \
        addr[8],  addr[9], addr[10], addr[11],\
        addr[12], addr[13],addr[14], addr[15]
#endif

