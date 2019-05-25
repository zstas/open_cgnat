#include "main.h"

struct bitarray* bitarray_init( uint16_t length )
{
    struct bitarray *new_array = rte_calloc( NULL, 1, sizeof( struct bitarray ), 0 );

    new_array->length = length;

    uint8_t length_in_bytes = length / 8;
    if( length % 8 > 0)
        length_in_bytes++;

    new_array->array = rte_calloc( NULL, length_in_bytes, sizeof( uint8_t ), 0 );

    return new_array;
}

void bitarray_free( struct bitarray* a )
{
    rte_free( a->array );
    rte_free( a );
}

int32_t bitarray_set_next_available_bit( struct bitarray *a )
{
    int32_t index = 0;

    while( index < ( a->length / 8 ) && a->array[ index ] == 0xFF )
        index++;

    if( ( ( a->array[ index ] >> 0 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 0; return index * 8 + 0; }
    if( ( ( a->array[ index ] >> 1 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 1; return index * 8 + 1; }
    if( ( ( a->array[ index ] >> 2 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 2; return index * 8 + 2; }
    if( ( ( a->array[ index ] >> 3 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 3; return index * 8 + 3; }
    if( ( ( a->array[ index ] >> 4 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 4; return index * 8 + 4; }
    if( ( ( a->array[ index ] >> 5 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 5; return index * 8 + 5; }
    if( ( ( a->array[ index ] >> 6 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 6; return index * 8 + 6; }
    if( ( ( a->array[ index ] >> 7 ) & 1 ) == 0 ) { a->array[ index ] |= 1 << 7; return index * 8 + 7; }

    return -1;
}

int8_t bitarray_clean_bit( struct bitarray *a, uint16_t b )
{
    if( b > a->length )
        return -1;

    uint8_t selected_byte = b / 8;
    
    uint8_t selected_offset = b % 8;
    a->array[ selected_byte ] ^= 1 << selected_offset;
    return 0;
}

void bitarray_print( struct bitarray *a )
{
    uint8_t length_in_bytes = a->length / 8;
    if( a->length % 8 > 0)
        length_in_bytes++;

    for( int i = 0; i < length_in_bytes; i++ )
    {
        RTE_LOG( INFO, MAIN, "%x\n", a->array[ i ]);
    }
}