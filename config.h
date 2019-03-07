#define CONFIG_FILE_NOT_EXIST           -1

#define CONFIG_SECTION_NETFLOW		1
#define CONFIG_SECTION_POOL		2
#define CONFIG_SECTION_CLASSIFIER	3
#define CONFIG_SECTION_DPDK		4
#define CONFIG_SECTION_FILTER		5

uint8_t startWith( char *str, const char *pre );
uint32_t parseIP( char *str );
uint16_t parse_uint16_t( char *str );
uint8_t parse_uint8_t( char *str );
void prepare_config( void );
void print_config( void );
int load_config( char * file );
char * parse_string( char *str );
void parse_eal_args( char *str );
