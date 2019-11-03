#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static uint32_t
ngx_murmur_hash2(const uint8_t *data, size_t len)
{
    uint32_t  h, k;

    h = 0 ^ len;

    while (len >= 4) {
        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        k *= 0x5bd1e995;
        k ^= k >> 24;
        k *= 0x5bd1e995;

        h *= 0x5bd1e995;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch (len) {
    case 3:
        h ^= data[2] << 16;
        /* fall through */
    case 2:
        h ^= data[1] << 8;
        /* fall through */
    case 1:
        h ^= data[0];
        h *= 0x5bd1e995;
    }

    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;

    return h;
}

static uint32_t
DJBHash(const uint8_t *str)
{
    uint32_t hash = 5381;

    while (*str){
        hash = ((hash << 5) + hash) + (*str++); /* times 33 */
    }
    hash &= ~(1 << 31); /* strip the highest bit */
    return hash;
}


struct maglev_hash {
    int  table_size;
    int  table[0];
};

static int*
maglev_permutation(const char **server, int server_size, int table_size)
{
    int  i, j, size, name_len, *permutation;
    uint32_t offset, skip;
    const char *name;

    size = server_size * table_size;
    permutation = malloc(sizeof(int) * size);

    if (permutation == NULL) {
        return NULL;
    }

    for (i = 0; i < server_size; i++) {
        
        name = server[i];
        name_len = strlen(name);
        offset = ngx_murmur_hash2(name, name_len) % table_size;
        skip = (DJBHash(name) % (table_size - 1)) + 1;
 
        for (j = 0; j < table_size; j++) {
            permutation[i*table_size + j] =  (offset + j * skip) % table_size;
        }
    }

    return permutation;
}

static bool
maglev_population(int *table, int *permutation, int server_size, int table_size)
{
    int i, j, pos, num;
    int *next;
    bool ret = false;

    num = 0;
    next = calloc(server_size, sizeof(int));
    if (next == NULL) {
        return false;
    }

    for (i = 0; i < table_size; i++) {
        table[i] = -1; 
    }

    while (1) {
        for (i = 0; i < server_size; i++) {
            for (j = next[i]; j < table_size; j++) {
                pos = permutation[i*table_size + j];
                next[i]++;
                if (table[pos] == -1) {
                    table[pos] = i; 
                    num++;
                    if (num == table_size) {
                        ret = true;
                        goto out;
                    }
                    break;
                } 
            }
        }
    }

out:
    free(next);
    return ret;
}


struct maglev_hash* 
maglev_new(const char **server, int server_size, int table_size)
{
    struct maglev_hash *mh;
    int *permutation;
    bool ret = false;

    if ((mh = malloc(sizeof(struct maglev_hash) + sizeof(int) * table_size)) == NULL)
        return NULL;
    }

    mh->table_size = table_size;
    if ((permutation = maglev_permutation(server, server_size, table_size)) != NULL)
        ret = maglev_population(mh->table, permutation, server_size, table_size);
        free(permutation);
    }

    if (ret == false) {
        free(mh);
        mh = NULL;
    }

    return mh;
}

static void
maglev_print(struct maglev_hash* mh)
{
    int i;

    for (i = 0; i < mh->table_size; i++) {
        printf("%d ", mh->table[i]);
    }

    printf("\n");
}

int main(int argc, char **argv)
{
    struct maglev_hash *mh;
    char **server;
    int server_size;
    int table_size;

    if (argc <= 2) {
        printf("%s table_size server-list\n", argv[0]);
        return 0;
    }

    server = argv + 2;
    server_size = argc - 2;
    table_size = atoi(argv[1]);

    mh = maglev_new((const char**)server, server_size, table_size);

    if (mh) {
        maglev_print(mh);
        free(mh);
    }

    return 0;
}
