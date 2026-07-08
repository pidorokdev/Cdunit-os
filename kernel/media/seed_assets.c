/*
 * Dunit OS - Embedded seed media assets (MP3/JPEG)
 */

#include "types.h"

#define _tmp_dunitos_seed_mp3 dunit_seed_mp3
#define _tmp_dunitos_seed_mp3_len dunit_seed_mp3_len
#include "seed_mp3.inc"
#undef _tmp_dunitos_seed_mp3
#undef _tmp_dunitos_seed_mp3_len

#define _tmp_dunitos_seed_jpg dunit_seed_jpg
#define _tmp_dunitos_seed_jpg_len dunit_seed_jpg_len
#include "seed_jpeg.inc"
#undef _tmp_dunitos_seed_jpg
#undef _tmp_dunitos_seed_jpg_len

/* Bootstrap images are compiled separately as .c files */
