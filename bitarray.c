#include "main.h"

/*
 * Init bit array:
 * start - first element
 * end - end element
 */
struct bitarray* bitarray_init( uint16_t start, uint16_t end )
{
    struct bitarray *new_array = rte_calloc( NULL, 1, sizeof( struct bitarray ), 0 );

    new_array->length = end - start;
    new_array->offset = start;

    uint8_t length_in_bytes = new_array->length / 8;
    if( new_array->length % 8 > 0)
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

    for( int i = 0; i < 8; i++ )
    {
        if( ( ( a->array[ index ] >> i ) & 1 ) != 0 )
            continue;
        
        a->array[ index ] |= 1 << i;
        return a->offset + index * 8 + i;
    }

    return -1;
}

int8_t bitarray_clean_bit( struct bitarray *a, uint16_t b )
{
    if( ( b > a->length + a->offset ) || ( b < a->offset ) )
        return -1;

    b -= a->offset;

    uint8_t selected_byte = b / 8;
    
    uint8_t selected_offset = b % 8;
    a->array[ selected_byte ] &= ~(1 << selected_offset );
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