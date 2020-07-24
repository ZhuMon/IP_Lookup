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
/*global variables*/
struct list *root;
int num_entry = 0;
int num_query = 0;
struct ENTRY *table, *query;
int N = 0;  // number of nodes
unsigned long long int begin, end, total = 0;
unsigned long long int *my_clock;
int num_node = 0;  // total number of nodes in the binary trie
////////////////////////////////////////////////////////////////////////////////////
struct list *create_node()
{
    struct list *temp;
    num_node++;
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
    int j;
    struct list *current = root, *temp = NULL;
    for (j = 31; j >= (-1); j--) {
        if (current == NULL)
            break;
        if (current->port != 256)
            temp = current;
        if (ip & (1 << j)) {
            current = current->right;
        } else {
            current = current->left;
        }
    }
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
void one_level_push(struct list *node, _Bool pushed)
{
    // 1. has no child: nothing
    // 2. has a child: push to another child
    // 3. has two children: delete
    if (!node) {
        return;
    }
    _Bool push_left = 0, push_right = 0;
    if (node->port != 256 && pushed == 0) {
        if (node->left && node->right) {
            if (node->left->port != 256 && node->right->port != 256) {
                node->port = 256;
            } else if (node->left->port != 256) {
                node->right->port = node->port;
                node->port = 256;
                push_right = 1;
            } else if (node->right->port != 256) {
                node->left->port = node->port;
                node->port = 256;
                push_left = 1;
            }
        } else if (node->left && node->left->port != 256) {
            node->right = (struct list *) malloc(sizeof(struct list));
            node->right->left = NULL;
            node->right->right = NULL;
            node->right->port = node->port;

            node->port = 256;
            push_right = 1;
            num_node++;
        } else if (node->right && node->right->port != 256) {
            node->left = (struct list *) malloc(sizeof(struct list));
            node->left->left = NULL;
            node->left->right = NULL;
            node->left->port = node->port;

            node->port = 256;
            push_left = 1;
            num_node++;
        }
    }

    one_level_push(node->left, push_left);
    one_level_push(node->right, push_right);
}
////////////////////////////////////////////////////////////////////////////////////
void create()
{
    int i;

    root = create_node();
    begin = rdtsc();
    for (i = 0; i < num_entry; i++)
        add_node(table[i].ip, table[i].len, table[i].port);
    end = rdtsc();
    free(table);
}
////////////////////////////////////////////////////////////////////////////////////
void count_node(struct list *r)
{
    if (r == NULL)
        return;
    count_node(r->left);
    if (r->port != 256)
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
    printf("(MaxClock, MinClock) =\t(%5llu, %5llu)\n", MaxClock, MinClock);

    for (i = 0; i < 50; i++) {
        printf("%d\n", NumCntClock[i]);
    }
    return;
}

void shuffle(struct ENTRY *array, int n)
{
    srand((unsigned) time(NULL));
    struct ENTRY *temp = (struct ENTRY *) malloc(sizeof(struct ENTRY));

	int i;
    for (i = 0; i < n - 1; i++) {
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
    printf("Avg. Insert:\t%llu\n", (end - begin) / num_entry);
    count_node(root);
    printf("number of prefix nodes:\t%d\n", N);
    one_level_push(root, 0);
    N = 0;
    count_node(root);
    printf("After level push:\t%d\n", N);
    printf("number of all node:\t%d\n", num_node);

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
    printf("Avg. Search:\t%llu\n", total / num_query);
    printf("Total memory requirement:\t%ld KB\n",
           ((num_node * sizeof(struct list)) / 1024));
    CountClock();
    ////////////////////////////////////////////////////////////////////////////
    // printf("There are %d nodes in binary trie\n", count_node(root));
    return 0;
}
