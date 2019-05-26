#include "main.h"

struct app_config global_config;
struct rte_mempool *mbuf_pool;
volatile sig_atomic_t reload;
rte_atomic64_t timestamp;

void init_ipc_to_lcores( void )
{
        char buf[128];

	for( int i = 0; i < global_config.rxs_count; i++ )
	{
		memset( buf, 0, sizeof( buf ) );
	        snprintf( buf, 128, "rx_from_main_%d", global_config.lcore_rx[i].id );
                global_config.lcore_rx[i].from_main = ring_wait_and_lookup( buf );
	        snprintf( buf, 128, "rx_to_main_%d", global_config.lcore_rx[i].id );
                global_config.lcore_rx[i].to_main = ring_wait_and_lookup( buf );
	}

	for( int i = 0; i < global_config.workers_count; i++ )
	{
		memset( buf, 0, sizeof( buf ) );
	        snprintf( buf, 128, "worker_from_main_%d", i);
                global_config.lcore_worker[i].from_main = ring_wait_and_lookup( buf );
	        snprintf( buf, 128, "worker_to_main_%d", i );
		global_config.lcore_worker[i].to_main = ring_wait_and_lookup( buf );
	}
}

void run_worker( uint8_t id )
{
	RTE_LOG( INFO, MAIN, "Starting worker %d...\n", id );
	for( int i = 0; i < global_config.rxs_count; i++ )
	{
		struct ipc_msg *msg = rte_malloc( NULL, sizeof( struct ipc_msg ), 0 );
		msg->type = IPC_RX_START_WORKER;
		msg->data[0] = id;
		int ret = rte_ring_enqueue( global_config.lcore_rx[i].from_main, (void*)msg );
		if( ret != 0 )
			RTE_LOG( ERR, MAIN, "IPC between threads is broken!\n" );
		while( rte_ring_dequeue( global_config.lcore_rx[i].to_main, (void*)&msg ) != 0 )
		{}
		if( msg->type == IPC_RX_STARTED_WORKER )
			RTE_LOG( INFO, MAIN, "Worker %d started on lcore_rx %d\n", id, i );
		else
			RTE_LOG( INFO, MAIN, "Incorrect response %x, from lcore_rx %d\n", msg->type, i );
		rte_free( msg );
	}
}

void stop_worker( uint8_t id )
{
	RTE_LOG( INFO, MAIN, "Stopping worker %d...\n", id );
	for( int i = 0; i < global_config.rxs_count; i++ )
	{
		struct ipc_msg *msg = rte_malloc( NULL, sizeof( struct ipc_msg ), 0 );
		msg->type = IPC_RX_STOP_WORKER;
		msg->data[0] = id;
		int ret = rte_ring_enqueue( global_config.lcore_rx[i].from_main, (void*)msg );
		if( ret != 0 )
			RTE_LOG( ERR, MAIN, "IPC between threads is broken!\n" );
		while( rte_ring_dequeue( global_config.lcore_rx[i].to_main, (void*)&msg ) != 0 )
		{}
		if( msg->type == IPC_RX_STOPPED_WORKER )
			RTE_LOG( INFO, MAIN, "Worker %d stopped on lcore_rx %d\n", id, i );
		else
			RTE_LOG( INFO, MAIN, "Incorrect response %x, from lcore_rx %d\n", msg->type, i );
		rte_free( msg );
	}
}

void reload_worker( uint8_t id )
{
	RTE_LOG( INFO, MAIN, "Reloading worker %d...", id );
	if( id >= global_config.workers_count )
		return;

	struct ipc_msg *msg = rte_malloc( NULL, sizeof( struct ipc_msg ), 0 );
	msg->type = IPC_WORKER_RELOAD;
	int ret = rte_ring_enqueue( global_config.lcore_worker[id].from_main, (void*)msg );
	if( ret != 0 )
		RTE_LOG( ERR, MAIN, "IPC between threads is broken!\n" );
	while( rte_ring_dequeue( global_config.lcore_worker[id].to_main, (void*)&msg ) != 0 )
	{}
	if( msg->type == IPC_WORKER_RESPONSE )
		RTE_LOG( INFO, MAIN, "Worker %d successfully reloaded\n", id );
	else
		RTE_LOG( INFO, MAIN, "Incorrect response %x, from worker %d\n", msg->type, id );
	rte_free( msg );
}

void reload_all_workers( void )
{
	for( int i = 0; i < global_config.workers_count; i++ )
	{
		stop_worker( i );
		reload_worker( i );
		run_worker( i );
	}
}

void reload_handler( __attribute__((unused)) int sig )
{
	reload = 1;
}

void lcore_main( void )
{
	rte_atomic64_init( &timestamp );
	init_ipc_to_lcores();

	for( int i = 0; i < global_config.workers_count; i++ )
	{
		run_worker( i );
	}

	struct sigaction act;
	memset( &act, 0, sizeof( act ) );
	act.sa_handler = reload_handler;

	if( sigaction( SIGHUP, &act, NULL ) < 0 )
		rte_exit( EXIT_FAILURE, "Cannot init handle SIGHUP" );


	while( 1 )
	{
		if( reload )
		{
			reload = 0;
			reload_all_workers();
		}

		sleep( 1 );
		rte_atomic64_set( &timestamp, time( NULL ) );
	}
}

void print_version( void )
{
	printf( "Current version: %s\n", VERSION );
	exit( 0 );
}

void print_help( void )
{
	printf( "To run use with this options:\n" );
	printf( " url_filter -c /path/to/config -d\n" );
	printf( "Argument description:\n" );
	printf( " -h, --help : print this message\n" );
	printf( " -c, --config : path to config file\n" );
	printf( " -d, --daemonize : run as daemon\n" );
	printf( " -v, --version : print version\n" );
	exit( 0 );
}

int parse_args( int argc, char ** argv )
{
	if( argc <= 1 )
		print_help();
	if( strcmp( "-h", argv[ 1 ] ) == 0 )
		print_help();
	if( strcmp( "--help", argv[ 1 ] ) == 0 )
		print_help();
	if( strcmp( "-v", argv[ 1 ] ) == 0 )
		print_version();
	if( strcmp( "--version", argv[ 1 ] ) == 0 )
		print_version();
	for( int i = 0; i < argc; i++ )
	{
		if( strcmp( "-c", argv[ i ] ) == 0 )
			load_config( argv[ i + 1 ] );
	}
	return 0;
}

int main( int argc, char ** argv )
{
	parse_args( argc, argv );

	int ret = rte_eal_init( global_config.eal_args_count, global_config.eal_args );
	if( ret < 0 )
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	rte_log_set_global_level( RTE_LOG_INFO );

	mbuf_pool = rte_pktmbuf_pool_create( "mbuf_pool", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id() );

	if( mbuf_pool == NULL )
		rte_exit( EXIT_FAILURE, "Cannot create mbuf pool\n" );

	if( rte_lcore_count() > 1 )
		RTE_LOG( INFO, MAIN, "%d cores is enabled.\n", rte_lcore_count() );

	ret = cgnat_init_pool( &global_config.pool_conf, &global_config.pool, 0 );
    if( ret < 0 )
            rte_exit(EXIT_FAILURE, "Error NAT POOL initialization with code %d\n", ret );

	schedule_lcores( global_config.mode );

	lcore_main();

	return 0;
}

int schedule_lcores( uint8_t lcore_sched_scheme )
{
	RTE_LOG( INFO, MAIN, "Schedule cores scheme is: %u\n", lcore_sched_scheme );
	uint16_t port_count = rte_eth_dev_count_avail();

	uint16_t current_lcore = 0;
	uint16_t lcore_txrx_count = 0;
	uint16_t lcore_tx_count = 0;
	uint16_t lcore_rx_count = 0;
	uint16_t lcore_worker_count = 0;

	switch( lcore_sched_scheme )
	{
		case ONE_TXRX:
			lcore_txrx_count = 1;
			break;
		case MANY_TXRX:
			lcore_txrx_count = port_count;
			break;
		case ONE_TX_ONE_RX:
			lcore_tx_count = 1;
			lcore_rx_count = 1;
			break;
		case MANY_TX_MANY_RX:
			lcore_tx_count = port_count;
			lcore_rx_count = port_count;
			break;
	}

	global_config.rxs_count = lcore_rx_count;
	global_config.lcore_rx = rte_malloc( NULL, sizeof( struct lcore_conf ) * global_config.rxs_count, 0 );

	if( rte_get_next_lcore( rte_lcore_id(), 1, 0 ) == RTE_MAX_LCORE )
		rte_exit( EXIT_FAILURE, "No lcores available\n" );

	int iterator = 0;

//	for( current_lcore = rte_get_next_lcore( rte_lcore_id(), 1, 0 ); current_lcore < RTE_MAX_LCORE; current_lcore = rte_get_next_lcore( current_lcore, 1, 0 ) )
	RTE_LCORE_FOREACH_SLAVE( current_lcore )
	{
		RTE_LOG( DEBUG, MAIN, "Schedule core %d\n", current_lcore );
		rte_eal_wait_lcore( current_lcore );

		if( lcore_txrx_count > 0 )
		{
			lcore_txrx_count--;
			port_count--;
			if( lcore_sched_scheme == ONE_TXRX )
			{
				rte_eal_remote_launch( (lcore_function_t *)lcore_txrx, NULL, current_lcore );
			}
			else
				rte_eal_remote_launch( (lcore_function_t *)lcore_txrx, ( void * )&port_count, current_lcore );
			continue;
		}

		if( lcore_tx_count > 0 )
		{
			lcore_tx_count--;
			port_count--;
			if( lcore_sched_scheme == ONE_TX_ONE_RX )
				rte_eal_remote_launch( (lcore_function_t *)lcore_tx, NULL, current_lcore );
			else
				rte_eal_remote_launch( (lcore_function_t *)lcore_tx_per_port, ( void * )&port_count, current_lcore );
			continue;
		}

		if( port_count == 0 )
			port_count = rte_eth_dev_count_avail();

		if( lcore_rx_count > 0 )
		{
			lcore_rx_count--;
			port_count--;
			global_config.lcore_rx[ iterator++ ].id = current_lcore;
			if( lcore_sched_scheme == ONE_TX_ONE_RX )
				rte_eal_remote_launch( (lcore_function_t *)lcore_rx, NULL, current_lcore );
			else
				rte_eal_remote_launch( (lcore_function_t *)lcore_rx_per_port, ( void * )&port_count, current_lcore );
			continue;
		}

		rte_eal_remote_launch( (lcore_function_t *)lcore_worker, ( void * )&lcore_worker_count, current_lcore );
		usleep( 10 );
		lcore_worker_count++;
	}
	global_config.workers_count = lcore_worker_count;
	RTE_LOG( INFO, MAIN, "Count of workerks: %d\n", lcore_worker_count );
	global_config.lcore_worker = rte_malloc( NULL, sizeof( struct lcore_conf ) * global_config.workers_count, 0 );

	return lcore_worker_count;
}

