#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
struct list {  // structure of binary trie
    unsigned int port;
    struct list *left, *right;
};
////////////////////////////////////////////////////////////////////////////////////
struct bitmap {
    unsigned exist : 15;
    unsigned child : 16;
    unsigned int port[15];
    struct bitmap *nexthop;
};
////////////////////////////////////////////////////////////////////////////////////
/*global variables*/
struct list *root;
int num_entry = 0;
int num_query = 0;
struct ENTRY *table, *query;
int N = 0;  // number of nodes
unsigned long long int begin, end, total = 0;
unsigned long long int *my_clock;
int num_node = 0;  // total number of nodes in the binary trie

struct bitmap *root_bitmap;
////////////////////////////////////////////////////////////////////////////////////
/* define functions */
void create_bitmap(struct bitmap *node, struct list *trie_node);
int count_ones(unsigned int num);
////////////////////////////////////////////////////////////////////////////////////
struct list *create_node()
{
    struct list *temp;
    temp = (struct list *) malloc(sizeof(struct list));
    temp->right = NULL;
    temp->left = NULL;
    temp->port = 256;  // default port
    return temp;
}
////////////////////////////////////////////////////////////////////////////////////
void add_node(unsigned int ip, unsigned char len, unsigned char nexthop)
{
    struct list *ptr = root;
    int i;
    for (i = 0; i < len; i++) {
        if (ip & (1 << (31 - i))) {
            if (ptr->right == NULL)
                ptr->right = create_node();  // Create Node
            ptr = ptr->right;
            if ((i == len - 1) && (ptr->port == 256))
                ptr->port = nexthop;
        } else {
            if (ptr->left == NULL)
                ptr->left = create_node();
            ptr = ptr->left;
            if ((i == len - 1) && (ptr->port == 256))
                ptr->port = nexthop;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////
void read_table(char *str, unsigned int *ip, int *len, unsigned int *nexthop)
{
    char tok[] = "./";
    char buf[100], *str1;
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
    *ip = n[0];
    *ip <<= 8;
    *ip += n[1];
    *ip <<= 8;
    *ip += n[2];
    *ip <<= 8;
    *ip += n[3];
}
////////////////////////////////////////////////////////////////////////////////////
void search(unsigned int ip)
{
    struct bitmap *now = root_bitmap;
    unsigned int i, four, three, two, one;
    unsigned int nexthop = 256;
    for (i = 32; i > 0; i -= 4) {
        four = (ip >> (i - 4)) & 0xf;
        three = four >> 1;
        two = four >> 2;
        one = four >> 3;
        // get the bit
        // first three bit of ip compare to last 8 bit of now->exist
        if ((((now->exist & 0xff) >> (8 - three - 1)) & 1) == 1) {
            // ignore -1 because of the count from 0 -> +1-1 = 0
            // shift more to ignore the bit to count down 1
            nexthop = now->port[8 - three - 1];
        } else if (((((now->exist & 0xf00) >> 8) >> (4 - two - 1)) & 1) == 1) {
            nexthop = now->port[12 - two - 1];
        } else if ((((now->exist & 0x3000) >> 12) >> (2 - one - 1)) == 1) {
            nexthop = now->port[14 - one - 1];
        } else if ((now->exist >> 14) == 1) {
            nexthop = now->port[0];
        }
        if (((now->child >> (16 - four - 1)) & 1) == 1) {
            if (sizeof(now->nexthop) == sizeof(struct bitmap)) {
                now = now->nexthop;
            } else {
                now = &now->nexthop[count_ones(now->child >> (16 - four))];
            }
        } else {
            break;
        }
    }
    /*
    if (nexthop == 256) {
        printf("ip: %x not found\n", ip);
    } else {
        printf("nexthop: %d\n", nexthop);
    }*/
    /*if(temp==NULL)
      printf("default\n");
      else
      printf("%u\n",temp->port);*/
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
    my_clock = (unsigned long long int *) malloc(num_query *
                                              sizeof(unsigned long long int));
    num_query = 0;
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        query[num_query].ip = ip;
        my_clock[num_query++] = 10000000;
    }
}
////////////////////////////////////////////////////////////////////////////////////
void create()
{
    int i;

    root = create_node();
    begin = rdtsc();
    for (i = 0; i < num_entry; i++)
        add_node(table[i].ip, table[i].len, table[i].port);
    free(table);
}
////////////////////////////////////////////////////////////////////////////////////
_Bool is_node_exist(struct list *node)
{
    if (!node || node->port == 256) {
        return 0;
    } else {
        return 1;
    }
}
////////////////////////////////////////////////////////////////////////////////////
int count_ones(unsigned int num)
{
    int c;
    for (c = 0; num; num >>= 1) {
        c += num & 1;
    }
    return c;
}
////////////////////////////////////////////////////////////////////////////////////
unsigned int recur_get_tree(struct list *node,
                            unsigned int which,
                            unsigned int exist,
                            struct bitmap *now_bitmap)
{
    if (!node || which > 14) {
        return exist;
    }


    exist += is_node_exist(node) << (14 - which);

    now_bitmap->port[which] = node->port;

    if (node->left) {
        exist = recur_get_tree(node->left, which * 2 + 1, exist, now_bitmap);
    }
    if (node->right) {
        exist = recur_get_tree(node->right, which * 2 + 2, exist, now_bitmap);
    }
    if (which > 6) {
        if (node->left) {
            now_bitmap->child += 1 << (which * 2 + 1 - 15);
            if (now_bitmap->child != 0) {
                if (!now_bitmap->nexthop) {
                    now_bitmap->nexthop =
                        (struct bitmap *) malloc(sizeof(struct bitmap));
                    create_bitmap(now_bitmap->nexthop, node->left);
                } else {
                    now_bitmap->nexthop = (struct bitmap *) realloc(
                        now_bitmap->nexthop,
                        sizeof(struct bitmap) * count_ones(now_bitmap->child));
                    create_bitmap(
                        &now_bitmap->nexthop[count_ones(now_bitmap->child) - 1],
                        node->left);
                }
            }
        }
        if (node->right) {
            now_bitmap->child += 1 << (which * 2 + 2 - 15);

            if (now_bitmap->child != 0) {
                if (!now_bitmap->nexthop) {
                    now_bitmap->nexthop =
                        (struct bitmap *) malloc(sizeof(struct bitmap));
                    create_bitmap(now_bitmap->nexthop, node->right);
                } else {
                    now_bitmap->nexthop = (struct bitmap *) realloc(
                        now_bitmap->nexthop,
                        sizeof(struct bitmap) * count_ones(now_bitmap->child));
                    create_bitmap(
                        &now_bitmap->nexthop[count_ones(now_bitmap->child) - 1],
                        node->right);
                }
            }
        }
    }
    return exist;
}
////////////////////////////////////////////////////////////////////////////////////
void create_bitmap(struct bitmap *node, struct list *trie_node)
{
    if (!node) {
        node = (struct bitmap *) malloc(sizeof(struct bitmap));
    }
    node->child = 0;
    node->exist = 0;
    node->nexthop = NULL;
    int i;
    for (i = 0; i < 15; i++) {
        node->port[i] = 256;
    }
    num_node++;
    node->exist = recur_get_tree(trie_node, 0, is_node_exist(trie_node), node);
}
////////////////////////////////////////////////////////////////////////////////////
void build_bitmap()
{
    root_bitmap = (struct bitmap *) malloc(sizeof(struct bitmap));
    create_bitmap(root_bitmap, root);
    end = rdtsc();
}

////////////////////////////////////////////////////////////////////////////////////
void count_node(struct list *r)
{
    if (r == NULL)
        return;
    count_node(r->left);
    N++;
    count_node(r->right);
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
    printf("(%5llu, %5llu)\n", MaxClock, MinClock);

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
    build_bitmap();
#ifdef debug
    printf("port: ");
    for (i = 0; i < 15; i++) {
        printf("%d ", root_bitmap->port[i]);
    }
    printf("\n");
#endif
    printf("Avg. Insert\nnumber of bitmap\nTotal memory requirement\n");
    printf("Avg. Search\n(MaxClock, MinClock)\n");
    printf("%llu\n", (end - begin) / num_entry);
    printf("%d\n", num_node);
    printf("%ld KB\n", ((num_node * sizeof(struct bitmap)) / 1024));

    shuffle(query, num_entry);
    ////////////////////////////////////////////////////////////////////////////
    for (j = 0; j < 100; j++) {
        for (i = 0; i < num_query; i++) {
            begin = rdtsc();
            search(query[i].ip);
            end = rdtsc();
            if (my_clock[i] > (end - begin))
                my_clock[i] = (end - begin);
        }
    }
    total = 0;
    for (j = 0; j < num_query; j++)
        total += my_clock[j];
    printf("%llu\n", total / num_query);
    CountClock();
    ////////////////////////////////////////////////////////////////////////////
    // count_node(root);
    // printf("There are %d nodes in binary trie\n",N);
    return 0;
}
