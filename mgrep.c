/**
 * \file
 * \brief
 * Fast pattern searchs over large binary files using mmap and boyer-moore.
 *
 * Copyrights
 *
 * Portions created or assigned to Joe Hildebrand are
 * Copyright (c) 2011 Joe Hildebrand.  All Rights Reserved.
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "boyer_moore.h"

void usage()
{
    fprintf(stderr, "Usage: mgrep [OPTION]... HEXPATTERN FILE...\n");
    fprintf(stderr, "Search for the sequence of bytes represented by HEXPATERN\n");
    fprintf(stderr, "in one or more large binary FILEs.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -a NUM   Output NUM bytes after the found pattern\n");
    fprintf(stderr, " -b NUM   Output NUM bytes before the found pattern\n");
    fprintf(stderr, " -c       Highlight the found pattern in color\n");
    fprintf(stderr, " -H       Do not convert HEXPATTERN from hex\n");

    exit(64);
}


uint8_t hchar(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }
    fprintf(stderr, "Invalid input character: %c\n", c);
    exit(1);
}

// calls malloc, please free result
// returns NULL on error
uint8_t *hex_decode(const char* word, size_t *hex_size)
{
    if (!word)
    {
        return NULL;
    }

    size_t wlen = strlen(word);
    if (wlen == 0)
    {
        return NULL;
    }

    if (wlen%2 != 0)
    {
        // 2 chars per octet
        return NULL;
    }

    size_t ret_size = wlen / 2;
    uint8_t *ret = (uint8_t*)malloc(ret_size);
    for (size_t i=0; i<ret_size; i++)
    {
        ret[i] = (hchar(word[2*i]) << 4) | hchar(word[2*i + 1]);
    }
    if (hex_size)
    {
        *hex_size = ret_size;
    }
    return ret;
}

#define LINE_SIZE 16
void print_match(const uint8_t *file, size_t offset, size_t pattern_size,
                 size_t before, size_t after, int color)
{
    size_t start = offset - before;
    size_t end = offset + pattern_size + after - 1;
    size_t start_pad = start % LINE_SIZE;
    size_t cur = start - start_pad;
    int i;
    char c;

    while (cur < end)
    {
        printf("%08zx ", cur);
        for (i=cur; i<(cur + LINE_SIZE); i++)
        {
            if ((i < start) || (i > end))
            {
                printf("   ");
            }
            else
            {
                if (color && (i >= offset) && (i < offset + pattern_size))
                {
                    printf(" \x1b[2;31m%02x\x1b[0m", file[i]);
                }
                else
                {
                    printf(" %02x", file[i]);
                }
            }
        }
        printf("  |");
        for (i=cur; i<(cur + LINE_SIZE); i++)
        {
            if ((i < start) || (i > end))
            {
                printf(" ");
            }
            else
            {
                c = file[i];
                if (color && (i >= offset) && (i < offset + pattern_size))
                {
                    printf("\x1b[2;31m");
                }
                if (isprint(c))
                {
                    printf("%c", file[i]);
                }
                else
                {
                    printf(".");
                }
                if (color && (i >= offset) && (i < offset + pattern_size))
                {
                    printf("\x1b[0m");
                }
            }
        }
        printf("|\n");
        cur += LINE_SIZE;
    }
    printf("\n");
}

int main(int argc, char *const argv[])
{
    size_t before = 16;
    size_t after = 16;
    int color = 0;
    bool hexlify = true;
    int ch;
    long ia, ib;
    while ((ch = getopt(argc, argv, "a:b:chH")) != -1)
    {
        switch(ch)
        {
            case 'a':
                ia = strtol(optarg, NULL, 10);
                if ((ia > 0) && (ia < 1024))
                {
                    after = (size_t)ia;
                }
                break;
            case 'b':
                ib = strtol(optarg, NULL, 10);
                if ((ib > 0) && (ib < 1024))
                {
                    before = (size_t)ib;
                }
                break;
            case 'c':
                color++;
                break;
            case 'H':
                hexlify = !hexlify;
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }

    if (!isatty(STDOUT_FILENO) && color)
    {
        color--;
    }

    argc -= optind;
    argv += optind;

    if (argc < 2)
    {
        usage();
    }

    size_t pattern_size = 0;
    uint8_t *pattern;
    if (hexlify)
    {
        pattern = hex_decode(argv[0], &pattern_size);
    }
    else
    {
        pattern_size = strlen(argv[0]);
        pattern = (uint8_t*)strndup(argv[0], pattern_size);
    }

    if (!pattern)
    {
        fprintf(stderr, "Invalid pattern\n");
    }

    // initialize boyer-moore patterns
    int *delta1 = (int*)malloc(ALPHABET_LEN * sizeof(int));
    int *delta2 = (int*)malloc(pattern_size * sizeof(int));
    make_delta1(delta1, pattern, pattern_size);
    make_delta2(delta2, pattern, pattern_size);

    size_t count = 0;
    size_t errors = 0;
    for (int f=1; f<argc; f++)
    {
        int first = 1;
        const char *file_name = argv[f];
        int fd = open(file_name, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Open error ");
            perror(file_name);
            errors++;
            continue;
        }

        // Get the file size, to make mmap search the whole thing
        struct stat file_stat;
        if (fstat(fd, &file_stat) != 0)
        {
            fprintf(stderr, "Stat error ");
            perror(file_name);
            errors++;
            close(fd); // ignore error
            continue;
        }

        size_t file_size = (size_t)file_stat.st_size;
        if ((file_size == 0) || !S_ISREG(file_stat.st_mode))
        {
            close(fd); // ignore error
            continue;
        }

        const uint8_t *file = mmap(0, file_size, PROT_READ, MAP_SHARED, fd, 0);
        if (file == MAP_FAILED)
        {
            fprintf(stderr, "Mmap error ");
            perror(file_name);
            errors++;
            close(fd); // ignore error
            continue;
        }


        // Find all matches, don't worry about overlaps
        size_t last = 0;
        size_t next = 0;
        while ((next = bm_search(file + last,
                                 file_size - last,
                                 pattern, pattern_size,
                                 delta1, delta2)) != NOT_FOUND)
        {
            if (first)
            {
                first = 0;
                printf("---- %s ----\n", file_name);
            }

            print_match(file, last + next, pattern_size, before, after, color);
            last += next + pattern_size;
            count++;
        }

        if (munmap((void*)file, file_size) != 0)
        {
            fprintf(stderr, "Unmap error ");
            perror(file_name);
            errors++;
        }

        if (close(fd) != 0)
        {
            fprintf(stderr, "Close error ");
            perror(file_name);
            errors++;
        }
    }

    free(delta2);
    free(delta1);
    free(pattern);

    if (count > 0)
    {
        return 0;
    }
    if (errors > 0)
    {
        return 2;
    }
    return 1;
}
