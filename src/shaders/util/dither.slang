module dither;

// Size of dither matrix
static const uint DITHER_SIZE = 8;
// Mean value of dither matrix (2^N - 1)/(2^(N+1))
static const float DITHER_OFFSET = float((1u << DITHER_SIZE) - 1) / float(1u << (DITHER_SIZE + 1));

#define DITHER_ELEM(a) float(a) / float(DITHER_SIZE * DITHER_SIZE) - DITHER_OFFSET
#define DITHER_ROW(a1, a2, a3, a4, a5, a6, a7, a8) \
{ DITHER_ELEM(a1), DITHER_ELEM(a2), DITHER_ELEM(a3), DITHER_ELEM(a4), \
  DITHER_ELEM(a5), DITHER_ELEM(a6), DITHER_ELEM(a7), DITHER_ELEM(a8) },

static const float DITHER_MATRIX[DITHER_SIZE][DITHER_SIZE] = {
	DITHER_ROW( 0, 32,  8, 40,  2, 34, 10, 42)
	DITHER_ROW(48, 16, 56, 24, 50, 18, 58, 26)
	DITHER_ROW(12, 44,  4, 36, 14, 46,  6, 38)
	DITHER_ROW(60, 28, 52, 20, 62, 30, 54, 22)
	DITHER_ROW( 3, 35, 11, 43,  1, 33,  9, 41)
	DITHER_ROW(51, 19, 59, 27, 49, 17, 57, 25)
	DITHER_ROW(15, 47,  7, 39, 13, 45,  5, 37)
	DITHER_ROW(63, 31, 55, 23, 61, 29, 53, 21)
};

#undef DITHER_ROW
#undef DITHER_ELEM

// Apply "ordered dithering" algorithm to color value
// assuming it is to be stored into 256-valued image
public float3 dither256(float3 color, uint2 pixel)
{
	uint2 p = pixel % DITHER_SIZE;
	return color + DITHER_MATRIX[p.x][p.y] / 255.0;
}
