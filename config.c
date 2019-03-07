#include "main.h"

extern struct app_config global_config;

uint8_t startWith( char *str, const char *pre )
{
        return strncmp( pre, str, strlen( pre ) ) == 0;
}

uint32_t parseIP( char *str )
{
	int ipv4_bytes[4];
	int ret = sscanf( str, "%d.%d.%d.%d", &ipv4_bytes[0], &ipv4_bytes[1], &ipv4_bytes[2], &ipv4_bytes[3] );

	if( ret == 4 )
	{
		uint32_t ipv4 = ( ipv4_bytes[0] << 24 ) + ( ipv4_bytes[1] << 16 ) + ( ipv4_bytes[2] << 8 ) + ipv4_bytes[3];
		return ipv4;
	}
	return 0;
}

uint16_t parse_uint16_t( char *str )
{
	uint16_t result;
	int ret = sscanf( str, "%hu", &result );

	if( ret == 1 )
		return result;

	return 0xFFFF;
}

uint8_t parse_uint8_t( char *str )
{
	uint8_t result;
	int ret = sscanf( str, "%hhu", &result );

	if( ret == 1 )
		return result;

	return 0xFF;
}

char * parse_string( char *str )
{
	char * temp = malloc( strlen( str ) + 1 );
	memset( temp, 0, strlen( str ) + 1 );
	strncpy( temp, str, strlen( str ) - 1);
	return temp;
}

void parse_eal_args( char *str )
{
	char * temp = malloc( strlen( str ) + 1 );
	memset( temp, 0, strlen( str ) + 1 );
	strncpy( temp, str, strlen( str ) - 1);

	char name[12] = "http_block";

	global_config.eal_args_count = 0;
	global_config.eal_args[ global_config.eal_args_count ] = malloc( strlen( name ) + 1 );
	memset( global_config.eal_args[ global_config.eal_args_count ], 0, strlen( name ) + 1 );
	strncpy( global_config.eal_args[ global_config.eal_args_count ], name, strlen( name ) );
	global_config.eal_args_count++;

	char * tok = strtok( temp, " " );
	while( tok != NULL )
	{
		global_config.eal_args[ global_config.eal_args_count ] = malloc( strlen( tok ) + 1 );
		memset( global_config.eal_args[ global_config.eal_args_count ], 0, strlen( tok ) + 1 );
		strncpy( global_config.eal_args[ global_config.eal_args_count ], tok, strlen( tok ) );
		//printf( "Parsing string %s\n", global_config.eal_args[ global_config.eal_args_count ] );
		global_config.eal_args_count++;
		tok = strtok( NULL, " " );
	}
}

void prepare_config( void )
{
//	global_config.netflow.template_timer = NETFLOW_TEMPLATE_TIMER;
	global_config.pool_conf.timeout_tcp_syn = CGNAT_XLATE_TIMEOUT_TCP_SYN;
	global_config.pool_conf.timeout_tcp_est = CGNAT_XLATE_TIMEOUT_TCP_EST;
	global_config.pool_conf.timeout_tcp_fin_res = CGNAT_XLATE_TIMEOUT_TCP_FIN_RES;
	global_config.pool_conf.timeout_tcp_udp = CGNAT_XLATE_TIMEOUT_TCP_UDP;
	global_config.pool_conf.timeout_tcp_icmp = CGNAT_XLATE_TIMEOUT_TCP_ICMP;
}

void print_config( void )
{
	printf( "Transmit and receive mode: %x\n", global_config.mode );
	printf( "EAL arguments count: %d\n", global_config.eal_args_count );
	for( int i = 0; i < global_config.eal_args_count; i++ )
	{
		printf( "Argument[%d]: %s ", i, global_config.eal_args[ i ] );
	}
	printf( "\n" );
	printf( "Input vlan: %d\n", global_config.vlan_in );
	printf( "Output vlan: %d\n", global_config.vlan_out );
	printf( "Pool range: " IPv4_BYTES_FMT " - " IPv4_BYTES_FMT "\n", IPv4_BYTES( global_config.pool_conf.ip_from ), IPv4_BYTES( global_config.pool_conf.ip_to ) );
	printf( "Allowed ports: %d - %d\n", global_config.pool_conf.port_from, global_config.pool_conf.port_to );
	printf( "PBA size: %d\n", 1 << global_config.pool_conf.pba_size );
	printf( "Maximum subscribers: %d\n", 1 << global_config.pool_conf.maximum_subscribers );
	printf( "Maximum xlations: %d\n", 1 << global_config.pool_conf.maximum_xlations );
	printf( "EIM: %d, EIF: %d\n", global_config.pool_conf.endpoint_independent_mapping, global_config.pool_conf.endpoint_independent_filtering );
}

int load_config( char * file )
{
	FILE * f = fopen( file, "r" );

	if( f == NULL )
		return CONFIG_FILE_NOT_EXIST;

	char line[ 2048 ];
	uint8_t section = 0;

	memset( &global_config, 0, sizeof( global_config ) );
	prepare_config();

	while( 1 )
	{
		memset( line, 0, sizeof( line ) );
		if( fgets( line, sizeof( line ), f ) == NULL )
			break;

		if( startWith( line, "[dpdk]" ) )
		{
			section = CONFIG_SECTION_DPDK;
			continue;
		}

		if( startWith( line, "[pool]" ) )
		{
			section = CONFIG_SECTION_POOL;
			continue;
		}

		if( startWith( line, "[classifier]" ) )
		{
			section = CONFIG_SECTION_POOL;
			continue;
		}

		char *key = strtok( line, " =" );
		if( key == NULL )
			continue;

		switch( section )
		{
			case CONFIG_SECTION_DPDK:
				if( startWith( key, "eal_args" ) )
				{
					char *value = strtok( NULL, "=" );
					parse_eal_args( value );
				}
				if( startWith( key, "mode" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.mode = parse_uint8_t( value );
				}
				if( startWith( key, "vlan_in" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.vlan_in = parse_uint16_t( value );
				}
				if( startWith( key, "vlan_out" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.vlan_out = parse_uint16_t( value );
				}
				break;
			case CONFIG_SECTION_POOL:
				if( startWith( key, "ip_from" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.ip_from = parseIP( value );
				}
				if( startWith( key, "ip_to" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.ip_to = parseIP( value );
				}
				if( startWith( key, "port_from" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.port_from = parse_uint16_t( value );
				}
				if( startWith( key, "port_to" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.port_to = parse_uint16_t( value );
				}
				if( startWith( key, "pba_size" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.pba_size = parse_uint8_t( value );
				}
				if( startWith( key, "maximum_xlations" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.maximum_xlations = parse_uint8_t( value );
				}
				if( startWith( key, "maximum_subscribers" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.maximum_subscribers = parse_uint8_t( value );
				}
				if( startWith( key, "endpoint_independent_mapping" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.endpoint_independent_mapping = parse_uint8_t( value );
				}
				if( startWith( key, "endpoint_independent_filtering" ) )
				{
					char *value = strtok( NULL, " =" );
					global_config.pool_conf.endpoint_independent_filtering = parse_uint8_t( value );
				}
				break;
			default:
				break;
		}

	}

	print_config();

	fclose( f );

	return 0;
}
