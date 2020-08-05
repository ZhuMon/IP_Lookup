#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define max(a, b) ((a) > (b)) ? (a) : (b)

////////////////////////////////////////////////////////////////////////////////////
struct ENTRY {
    unsigned int ip;
    unsigned char len;
    unsigned char port;
};
////////////////////////////////////////////////////////////////////////////////////
static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
}

////////////////////////////////////////////////////////////////////////////////////
struct spread {
    int layer;
    int port;
    int *child;
    int *cnha;  // compressed next hop array
};
////////////////////////////////////////////////////////////////////////////////////
/*global variables*/
struct spread *root;
int num_entry = 0;
int num_query = 0;
struct ENTRY *table, *query;
int N = 0;  // number of nodes
unsigned long long int begin, end, total = 0;
unsigned long long int *my_clock;
////////////////////////////////////////////////////////////////////////////////////
struct spread *create_spread()
{
    struct spread *tmp;
    tmp = (struct spread *) malloc(sizeof(struct spread) * 65536);
    int i;
    for (i = 0; i < 65536; i++) {
        tmp[i].child = NULL;
        tmp[i].port = 256;
        tmp[i].layer = 0;
    }
    return tmp;
}
////////////////////////////////////////////////////////////////////////////////////
int *create_node(int layer)
{
    int *tmp;
    int size = pow(2, layer);
    tmp = (int *) malloc(sizeof(int) * size);
    int i;
    for (i = 0; i < size; i++){
        tmp[i] = 256;
    }
    return tmp;
}
////////////////////////////////////////////////////////////////////////////////////
void add_node(unsigned int ip, unsigned char len, unsigned char nexthop)
{
    struct spread *ptr = root;
    
    unsigned int n = ip >> 16;
    int i,j;
    if (len <= 16) {
        for (i = 0; i < pow(2, (16 - len)); i++) {
            ptr[n + i].port = nexthop;
        }
    } else {
        int old_layer = ptr[n].layer;
        int new_layer = len-16;

        if (ptr[n].child == NULL){
            ptr[n].child = create_node(new_layer);
            old_layer = new_layer;
        }

        if (old_layer < new_layer){
            // realloc
            int *new_child = create_node(new_layer);
            int sub = pow(2, (new_layer - old_layer));
            int old_poly = pow(2, old_layer);
            for (i = 0; i < old_poly; i++){
                for (j = 0; j < sub; j++) {
                    new_child[i * sub + j] = ptr[n].child[i];
                }
            }

            //printf("sub: %d old_poly: %d\n", sub, old_poly);
            //printf("new_child: %d\n", new_child[old_poly]);
            new_child[(ip & 0xffff) >> (32 - len)] = nexthop;
            //free(ptr[n].child);
            ptr[n].child = new_child;

            ptr[n].layer = new_layer;
        } else if (old_layer > new_layer) {
            int m = ((ip & 0xffff) >> (32 - len)) << (old_layer - new_layer);
            for (i = 0; i < pow(2, old_layer-new_layer); i++){
                ptr[n].child[m + i] = nexthop;
            }
        } else {
            int m = ((ip & 0xffff) >> (32 - len));
            ptr[n].child[m] = nexthop;
            //printf("ip & 0xffff : %x m: %d\n", (ip&0xffff), m);
            ptr[n].layer = new_layer;
        }

#ifdef debug
        printf("ip: %x\n", ip);
        for (i = 0; i < pow(2, max(old_layer, new_layer)); i++) {
            printf("%d", ptr[n].child[i]);
            if((i+1) % 64 == 0)
                printf("\n");
        }
        printf("\n");
        printf("======================\n");
#endif
    }
}
////////////////////////////////////////////////////////////////////////////////////
void read_table(char *str, unsigned int *ip, int *len, unsigned int *nexthop)
{
    char tok[] = "./";
    char buf[5], *str1;
    unsigned int n[4];
    sprintf(buf, "%s", strtok(str, tok));
    n[0] = atoi(buf);
    sprintf(buf, "%s", strtok(NULL, tok));
    n[1] = atoi(buf);
    sprintf(buf, "%s", strtok(NULL, tok));
    n[2] = atoi(buf);
    sprintf(buf, "%s", strtok(NULL, tok));
    n[3] = atoi(buf);
    *nexthop = n[2];
    str1 = (char *) strtok(NULL, tok);
    if (str1 != NULL) {
        sprintf(buf, "%s", str1);
        *len = atoi(buf);
    } else {
        if (n[1] == 0 && n[2] == 0 && n[3] == 0)
            *len = 8;
        else if (n[2] == 0 && n[3] == 0)
            *len = 16;
        else if (n[3] == 0)
            *len = 24;
    }
    *ip = (n[0] << 24) + (n[1] << 16) + (n[2] << 8) + n[3];
}
////////////////////////////////////////////////////////////////////////////////////
int search(unsigned int ip)
{
    int first16 = ip >> 16;
    int last16 = ip & 0xffff;
    int res = 256;
    if (root[first16].port != 256) {
        res = root[first16].port;
    }
    if (root[first16].child) {
        int i, count = 0;
        int m = (0xffff & ip) >> (16 - root[first16].layer);
        for (i = 0; i <= m; i++) {
            if (root[first16].child[i] == 1) {
                count++;
            }
        }
        res = (root[first16].cnha[count - 1] == 256)
                  ? res
                  : (root[first16].cnha[count - 1]);
    }

    return res;
}
////////////////////////////////////////////////////////////////////////////////////
void set_table(char *file_name)
{
    FILE *fp;
    int len;
    char string[100];
    unsigned int ip, nexthop;
    fp = fopen(file_name, "r");
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        num_entry++;
    }
    rewind(fp);
    table = (struct ENTRY *) malloc(num_entry * sizeof(struct ENTRY));
    num_entry = 0;
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        table[num_entry].ip = ip;
        table[num_entry].port = nexthop;
        table[num_entry++].len = len;
    }
}
////////////////////////////////////////////////////////////////////////////////////
void set_query(char *file_name)
{
    FILE *fp;
    int len;
    char string[100];
    unsigned int ip, nexthop;
    fp = fopen(file_name, "r");
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        num_query++;
    }
    rewind(fp);
    query = (struct ENTRY *) malloc(num_query * sizeof(struct ENTRY));
    my_clock = (unsigned long long int *) malloc(
        num_query * sizeof(unsigned long long int));
    num_query = 0;
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        query[num_query].ip = ip;
        query[num_query].port = nexthop;
        query[num_query].len = len;
        my_clock[num_query++] = 10000000;
    }
}
////////////////////////////////////////////////////////////////////////////////////
void compress(struct spread *ptr)
{
    if (!ptr->child)
        return;

    int size = pow(2, ptr->layer);
    int *new_child = (int *) malloc(sizeof(int) * size);
    int i, count = 0;
    int now = ptr->child[0];
    new_child[0] = 1;
    for (i = 1; i < size; i++) {
        if (ptr->child[i] == now) {
            new_child[i] = 0;
        } else {
            new_child[i] = 1;
            now = ptr->child[i];
            count++;
        }
    }
    ptr->cnha = (int *) malloc(sizeof(int) * (count + 1));
    int j = 0;
    for (i = 0; i < size; i++) {
        if (new_child[i] == 1) {
            ptr->cnha[j++] = ptr->child[i];
        }
    }
    ptr->child = new_child;
#ifdef debug
    for (i = 0; i < size; i++) {
        printf("%d", ptr->child[i]);
        if ((i + 1) % 64 == 0)
            printf("\n");
    }
    printf("\n");
    printf("----------------------\n");
#endif
}
////////////////////////////////////////////////////////////////////////////////////
void create()
{
    int i;
    root = create_spread();
    begin = rdtsc();
    for (i = 0; i < num_entry; i++) {
        add_node(table[i].ip, table[i].len, table[i].port);
    }
    for (i = 0; i < 65536; i++) {
        compress(&root[i]);
    }
    end = rdtsc();
    free(table);
}
////////////////////////////////////////////////////////////////////////////////////
void CountClock()
{
    unsigned int i;
    unsigned int *NumCntClock =
        (unsigned int *) malloc(50 * sizeof(unsigned int));
    for (i = 0; i < 50; i++)
        NumCntClock[i] = 0;
    unsigned long long MinClock = 10000000, MaxClock = 0;
    for (i = 0; i < num_query; i++) {
        if (my_clock[i] > MaxClock)
            MaxClock = my_clock[i];
        if (my_clock[i] < MinClock)
            MinClock = my_clock[i];
        if (my_clock[i] / 100 < 50)
            NumCntClock[my_clock[i] / 100]++;
        else
            NumCntClock[49]++;
    }
    printf("(MaxClock, MinClock) = (%5llu, %5llu)\n", MaxClock, MinClock);

    for (i = 0; i < 50; i++) {
        printf("%d\n", NumCntClock[i]);
    }
    return;
}

void shuffle(struct ENTRY *array, int n)
{
    srand((unsigned) time(NULL));
    struct ENTRY *temp = (struct ENTRY *) malloc(sizeof(struct ENTRY));

    for (int i = 0; i < n - 1; i++) {
        size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
        temp->ip = array[j].ip;
        temp->len = array[j].len;
        temp->port = array[j].port;
        array[j].ip = array[i].ip;
        array[j].len = array[i].len;
        array[j].port = array[i].port;
        array[i].ip = temp->ip;
        array[i].len = temp->len;
        array[i].port = temp->port;
    }
}
////////////////////////////////////////////////////////////////////////////////////
int cal_memory()
{
    int i, count_layer = 0, j, count;
    int mem = 0;
    for (i = 0; i < 65536; i++) {
        if (root[i].layer > 0) {
            mem += sizeof(_Bool) * pow(2, root[i].layer);
            count = 0;
            for (j = 0; j < pow(2, root[i].layer); j++) {
                count += (root[i].child[j] == 1) ? 1 : 0;
            }
            mem += sizeof(int) * count;
        }
    }
    mem += sizeof(struct spread) * 65536;
    return mem / 1024;
}
////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    /*if(argc!=3){
        printf("Please execute the file as the following way:\n");
        printf("%s  routing_table_file_name  query_table_file_name\n",argv[0]);
        exit(1);
    }*/
    int i, j;
    // set_query(argv[2]);
    // set_table(argv[1]);
    set_query(argv[1]);
    set_table(argv[1]);
    create();
    printf("Avg. Insert: %llu\n", (end - begin) / num_entry);
    printf("Total memory requirement: %d KB\n", cal_memory());

    shuffle(query, num_entry);
    ////////////////////////////////////////////////////////////////////////////
    for (j = 0; j < 100; j++) {
        for (i = 0; i < num_query; i++) {
            begin = rdtsc();
            if (search(query[i].ip) == 256) {
                printf("Not found: %x\n", query[i].ip);
            }
            end = rdtsc();
            if (my_clock[i] > (end - begin))
                my_clock[i] = (end - begin);
        }
    }
    total = 0;
    for (j = 0; j < num_query; j++)
        total += my_clock[j];
    printf("Avg. Search: %llu\n", total / num_query);
    CountClock();
    ////////////////////////////////////////////////////////////////////////////
    // printf("There are %d nodes in binary trie\n",N);
    return 0;
}
