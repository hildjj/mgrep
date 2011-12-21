#ifndef BOYER_MOORE_H
#define BOYER_MOORE_H

#define NOT_FOUND ((size_t)-1)
#define ALPHABET_LEN 256

void make_delta1(int *delta1, const uint8_t *pat, int32_t patlen);

void make_delta2(int *delta2, const uint8_t *pat, int32_t patlen);

size_t bm_search(const uint8_t *string, size_t stringlen,
                 const uint8_t *pat, size_t patlen,
                 int *delta1, int *delta2);

#endif // BOYER_MOORE_H