#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bitmap {
    unsigned char *data;
    int width;
    int height;
};

struct bitmap * bitmap_new(int width, int height, int value) {
	struct bitmap *bmp = (struct bitmap *)malloc(sizeof(struct bitmap));
    bmp->width = width;
    bmp->height = height;
    int size = (width * height + 7) / 8; // Calculate total bytes needed
    bmp->data = (unsigned char *)calloc(size,1 );
    memset(bmp->data, value ? 0xFF : 0x00, size);
	return bmp;
}

int bitmap_set(struct bitmap *bmp, int x, int y, int value) {
    if (x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) {
        return -1; // Return error code if coordinates are out of bounds
    }
    int idx = y * bmp->width + x;
    if (value)
        bmp->data[idx / 8] |= (1 << (idx % 8));
    else
        bmp->data[idx / 8] &= ~(1 << (idx % 8));
    return 0;
}

int bitmap_get(struct bitmap *bmp, int x, int y) {
    if (x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) {
        return -1; // Return error code if coordinates are out of bounds
    }
    int idx = y * bmp->width + x;
    return (bmp->data[idx / 8] & (1 << (idx % 8))) != 0;
}

void bitmap_free(struct bitmap *bmp) {
	if(bmp)
	{
		if(bmp->data)free(bmp->data);
		free(bmp);
	}
}




int test_bitmap() {
    struct bitmap *bmp = bitmap_new(10, 5, 1); // Create a 10x5 bitmap
    if (bitmap_set(bmp, 2, 2, 1) == 0) { // Set bit at position (2,2)
        printf("Bit at (2,2): %d\n", bitmap_get(bmp, 2, 2)); // Get bit at position (2,2)
    }
    bitmap_free(bmp);
    return 0;
}
