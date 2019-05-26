struct bitarray
{
    uint8_t *array;
    uint16_t length;
    uint16_t offset;
};

struct bitarray* bitarray_init( uint16_t length, uint16_t offset );
void bitarray_free( struct bitarray* a );
int32_t bitarray_set_next_available_bit( struct bitarray *a );
int8_t bitarray_clean_bit( struct bitarray *a, uint16_t b );
void bitarray_print( struct bitarray *a );