struct bitmap;
struct bitmap * bitmap_new(int width, int height, int value);
int bitmap_set(struct bitmap *bmp, int x, int y, int value);
int bitmap_get(struct bitmap *bmp, int x, int y);
void bitmap_free(struct bitmap *bmp);

int bitmap_is_all_zero(struct bitmap *bmp, int x, int y, int length);