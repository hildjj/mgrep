// Taken from:
// http://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string_search_algorithm
// and refactored slightly
// See: http://en.wikipedia.org/wiki/Wikipedia:Copyrights

#include <stdint.h>
#include <stdlib.h>

#include "boyer_moore.h"

#define max(a, b) ((a < b) ? b : a)

// delta1 table: delta1[c] contains the distance between the last
// character of pat and the rightmost occurence of c in pat.
// If c does not occur in pat, then delta1[c] = patlen.
// If c is at string[i] and c != pat[patlen-1], we can
// safely shift i over by delta1[c], which is the minimum distance
// needed to shift pat forward to get string[i] lined up
// with some character in pat.
// this algorithm runs in alphabet_len+patlen time.
void make_delta1(int *delta1, const uint8_t *pat, int32_t patlen)
{
    int i;
    for (i=0; i < ALPHABET_LEN; i++)
    {
        delta1[i] = patlen;
    }
    for (i=0; i < patlen-1; i++)
    {
        delta1[pat[i]] = patlen-1 - i;
    }
}

// true if the suffix of word starting from word[pos] is a prefix
// of word
static int is_prefix(const uint8_t *word, int wordlen, int pos)
{
    int i;
    int suffixlen = wordlen - pos;
    // could also use the strncmp() library function here,
    // if we didn't want to find nulls
    for (i = 0; i < suffixlen; i++)
    {
        if (word[i] != word[pos+i])
        {
            return 0;
        }
    }
    return 1;
}

// length of the longest suffix of word ending on word[pos].
// suffix_length("dddbcabc", 8, 4) = 2
static int suffix_length(const uint8_t *word, int wordlen, int pos)
{
    int i;
    // increment suffix length i to the first mismatch or beginning
    // of the word
    for (i = 0; (word[pos-i] == word[wordlen-1-i]) && (i < pos); i++);
    return i;
}

// delta2 table: given a mismatch at pat[pos], we want to align
// with the next possible full match could be based on what we
// know about pat[pos+1] to pat[patlen-1].
//
// In case 1:
// pat[pos+1] to pat[patlen-1] does not occur elsewhere in pat,
// the next plausible match starts at or after the mismatch.
// If, within the substring pat[pos+1 .. patlen-1], lies a prefix
// of pat, the next plausible match is here (if there are multiple
// prefixes in the substring, pick the longest). Otherwise, the
// next plausible match starts past the character aligned with
// pat[patlen-1].
//
// In case 2:
// pat[pos+1] to pat[patlen-1] does occur elsewhere in pat. The
// mismatch tells us that we are not looking at the end of a match.
// We may, however, be looking at the middle of a match.
//
// The first loop, which takes care of case 1, is analogous to
// the KMP table, adapted for a 'backwards' scan order with the
// additional restriction that the substrings it considers as
// potential prefixes are all suffixes. In the worst case scenario
// pat consists of the same letter repeated, so every suffix is
// a prefix. This loop alone is not sufficient, however:
// Suppose that pat is "ABYXCDEYX", and text is ".....ABYXCDEYX".
// We will match X, Y, and find B != E. There is no prefix of pat
// in the suffix "YX", so the first loop tells us to skip forward
// by 9 characters.
// Although superficially similar to the KMP table, the KMP table
// relies on information about the beginning of the partial match
// that the BM algorithm does not have.
//
// The second loop addresses case 2. Since suffix_length may not be
// unique, we want to take the minimum value, which will tell us
// how far away the closest potential match is.
void make_delta2(int *delta2, const uint8_t *pat, int32_t patlen)
{
    int p;
    int last_prefix_index = patlen-1;

    // first loop
    for (p=patlen-1; p>=0; p--)
    {
        if (is_prefix(pat, patlen, p+1))
        {
            last_prefix_index = p+1;
        }
        delta2[p] = last_prefix_index + (patlen-1 - p);
    }

    // second loop
    for (p=0; p < patlen-1; p++)
    {
        int slen = suffix_length(pat, patlen, p);
        if (pat[p - slen] != pat[patlen-1 - slen])
        {
            delta2[patlen-1 - slen] = patlen-1 - p + slen;
        }
    }
}

size_t bm_search(const uint8_t *string, size_t stringlen,
                 const uint8_t *pat, size_t patlen,
                 int *delta1, int *delta2)
{
    size_t i = patlen-1;
    while (i < stringlen)
    {
        int j = patlen-1;
        while (j >= 0 && (string[i] == pat[j]))
        {
            --i;
            --j;
        }
        if (j < 0)
        {
            return i + 1;
        }

        i += max(delta1[string[i]], delta2[j]);
    }
    return NOT_FOUND;
}
