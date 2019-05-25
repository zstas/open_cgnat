struct bitarray
{
    uint8_t *array;
    uint16_t length;
};

struct bitarray* bitarray_init( uint16_t length );
void bitarray_free( struct bitarray* a );
int32_t bitarray_set_next_available_bit( struct bitarray *a );
int8_t bitarray_clean_bit( struct bitarray *a, uint16_t b );
void bitarray_print( struct bitarray *a );