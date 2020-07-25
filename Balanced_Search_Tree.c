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
struct balanced_node {
    unsigned char port;
    int BF;
    char *prefix;
    struct balanced_node *left, *right;
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
struct balanced_node **bt_root;  // an array of pointer
int total_layer = 1;             // {0, 1}
////////////////////////////////////////////////////////////////////////////////////
int cover(char *a, char *b)
{
    for (; *a && *b; a++, b++) {
        if (*a != *b) {
            return 0;
        }
    }
    if (*b) {  // a cover b (len(b) > len(a))
        return 1;
    } else if (*a) {  // b cover a (len(a) > len(b))
        return -1;
    }
    perror("cover failed\n");
    exit(-1);
}
////////////////////////////////////////////////////////////////////////////////////
int compare_prefix(char *a, char *b)
{
    for (; *a && *b; a++, b++) {
        if (*a == *b)
            continue;
        else if (*a > *b)
            return 1;
        else
            return -1;
    }
    return 0;  // cover or equal
}
////////////////////////////////////////////////////////////////////////////////////
int cal_BF(struct balanced_node *node, int layer)
{
    if (!node) {
        return layer - 1;
    }

    int left_layer = cal_BF(node->left, layer + 1);
    int right_layer = cal_BF(node->right, layer + 1);

    node->BF = left_layer - right_layer;
    return (left_layer >= right_layer) ? left_layer : right_layer;
}
struct balanced_node *find_highest_wrong_BF(struct balanced_node *node,
                                            struct balanced_node *target,
                                            struct balanced_node *wrong)
{
    if (node && (node->BF > 1 || node->BF < -1)) {
        wrong = node;
    }
    if (node->left && compare_prefix(node->prefix, target->prefix) > 0) {
        wrong = find_highest_wrong_BF(node->left, target, wrong);
    } else if (node->right &&
               compare_prefix(node->prefix, target->prefix) < 0) {
        wrong = find_highest_wrong_BF(node->right, target, wrong);
    }
    return wrong;
}
struct balanced_node *find_parent(int layer, struct balanced_node *target)
{
    if (!target) {
        return NULL;
    }
    if (bt_root[layer] == target) {
        return bt_root[layer];
    }
    struct balanced_node *parent = bt_root[layer];
    while (parent->left != target && parent->right != target) {
        if (parent->left &&
            compare_prefix(parent->prefix, target->prefix) > 0) {
            parent = parent->left;
        } else if (parent->right &&
                   compare_prefix(parent->prefix, target->prefix) < 0) {
            parent = parent->right;
        } else {
            perror("Find parent failed!!!\n");
            exit(-1);
        }
    }
    return parent;
}
void rotate(int layer, struct balanced_node *target)
{
    cal_BF(bt_root[layer], 0);
    struct balanced_node *start, *node2, *node3, *parent, *mid;
    start = find_highest_wrong_BF(bt_root[layer], target, NULL);

    parent = find_parent(layer, start);
    while (start != NULL) {
        if (start->left && compare_prefix(start->prefix, target->prefix) > 0) {
            node2 = start->left;
            if (start->left->left &&
                compare_prefix(start->left->prefix, target->prefix) > 0) {
                // LL
                start->left = node2->right;
                node2->right = start;

                mid = node2;
            } else if (start->left->right &&
                       compare_prefix(start->left->prefix, target->prefix) <
                           0) {
                // LR
                node3 = node2->right;

                node2->right = node3->left;
                start->left = node3->right;

                node3->left = node2;
                node3->right = start;

                mid = node3;
            }
        } else if (start->right &&
                   compare_prefix(start->prefix, target->prefix) < 0) {
            node2 = start->right;
            if (start->right->left &&
                compare_prefix(start->right->prefix, target->prefix) > 0) {
                // RL
                node3 = node2->left;

                node2->left = node3->right;
                start->right = node3->left;

                node3->right = node2;
                node3->left = start;

                mid = node3;
            } else if (start->right->right &&
                       compare_prefix(start->right->prefix, target->prefix) <
                           0) {
                // RR
                start->right = node2->left;
                node2->left = start;

                mid = node2;
            }
        } else {
            perror("rotate error\n");
            exit(-1);
        }

        if (parent->left == start) {
            parent->left = mid;
        } else if (parent->right == start) {
            parent->right = mid;
        } else if (parent == start) {  // start == root
            bt_root[layer] = mid;
        } else {
            perror("Find parent failed\n");
            exit(-1);
        }

        cal_BF(bt_root[layer], 0);
        start = find_highest_wrong_BF(bt_root[layer], target, NULL);
    }
}
char *convert_to_prefix(unsigned int ip, unsigned int len)
{
    int i;
    char *prefix = (char *) malloc(len + 1);
    for (i = 0; i < len; i++) {
        prefix[i] = ((ip >> (31 - i)) & 1) ? '1' : '0';
    }
    prefix[i] = '\0';
    return prefix;
}
void print_BF(struct balanced_node *node, int layer)
{
    if (!node) {
        return;
    }
    int i;
    for (i = 0; i < layer; i++) {
        printf("\t");
    }
    printf("%d", node->BF);
    printf("\n");
    print_BF(node->left, layer + 1);
    print_BF(node->right, layer + 1);
}
void insert_to_balanced_tree(int layer, char *prefix, unsigned char nexthop)
{
    /*printf("BF: --- %d\n", bt_root[layer]->BF);*/
    /*print_BF(bt_root[layer], 0);*/
    if (layer > total_layer) {
        bt_root = (struct balanced_node **) realloc(
            bt_root, sizeof(struct balanced_node *) * (layer + 1));
        bt_root[layer] =
            (struct balanced_node *) malloc(sizeof(struct balanced_node));
        printf("realloc : %d layer\n", layer);
        total_layer = layer;
        bt_root[layer]->prefix = NULL;
    }
    if (bt_root[layer]->prefix == NULL) {
        bt_root[layer]->port = nexthop;
        bt_root[layer]->prefix = prefix;
        bt_root[layer]->left = NULL;
        bt_root[layer]->right = NULL;
        bt_root[layer]->BF = 0;
        return;
    }
    struct balanced_node *now;
    now = bt_root[layer];
    struct balanced_node *new =
        (struct balanced_node *) malloc(sizeof(struct balanced_node));
    new->port = nexthop;
    new->prefix = prefix;
    new->left = NULL;
    new->right = NULL;
    new->BF = 0;
    while (now) {
        // cover exist node -> level up
        if (cover(prefix, now->prefix) > 0) {
            free(new);
            insert_to_balanced_tree(layer + 1, prefix, nexthop);
            break;
        } else if (cover(prefix, now->prefix) < 0) {
            insert_to_balanced_tree(layer + 1, now->prefix, now->port);
            now->prefix = prefix;
            now->port = nexthop;
            break;
        }
        if (compare_prefix(prefix, now->prefix) < 0) {
            if (!now->left) {
                now->left = new;
                rotate(layer, new);
                break;
            } else {
                now = now->left;
            }
        } else if (compare_prefix(prefix, now->prefix) > 0) {
            if (!now->right) {
                now->right = new;
                rotate(layer, new);
                break;
            } else {
                now = now->right;
            }
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
void create()
{
    int i;

    bt_root =
        (struct balanced_node **) malloc(sizeof(struct balanced_node *) * 2);
    bt_root[0] = (struct balanced_node *) malloc(sizeof(struct balanced_node));
    bt_root[1] = (struct balanced_node *) malloc(sizeof(struct balanced_node));
    bt_root[0]->prefix = NULL;
    bt_root[1]->prefix = NULL;
    begin = rdtsc();
    for (i = 0; i < num_entry; i++) {
        insert_to_balanced_tree(0, convert_to_prefix(table[i].ip, table[i].len),
                                table[i].port);
    }
    end = rdtsc();
    free(table);
}
////////////////////////////////////////////////////////////////////////////////////
int search(char *ip)
{
    int j;
    struct balanced_node *now;
    int res;

    for (j = 0; j <= total_layer; j++) {
        now = bt_root[j];

        while (now && now->prefix) {
            res = compare_prefix(ip, now->prefix);
            if (res == 1)
                now = now->right;
            else if (res == -1)
                now = now->left;
            else
                return now->port;
        }
    }
    return 256;
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
    my_clock = (unsigned long long int *) malloc(
        num_query * sizeof(unsigned long long int));
    num_query = 0;
    while (fgets(string, 50, fp) != NULL) {
        read_table(string, &ip, &len, &nexthop);
        query[num_query].ip = ip;
        my_clock[num_query++] = 10000000;
    }
}
////////////////////////////////////////////////////////////////////////////////////
/*void count_node(struct list *r)
{
    if (r == NULL)
        return;
    count_node(r->left);
    N++;
    count_node(r->right);
}*/
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
void print_bt(struct balanced_node *node, int layer)
{
    if (!node || !(node->prefix)) {
        return;
    }
    int i;
    for (i = 0; i < layer; i++) {
        printf("\t");
    }
    printf("%s", node->prefix);
    printf("\n");
    print_bt(node->left, layer + 1);
    print_bt(node->right, layer + 1);
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

    /* for (i = 0; i <= total_layer; i++) {*/
    /*printf("layer%d ---------\n", i);*/
    /*print_bt(bt_root[i], 0);*/
    /*}*/

    shuffle(query, num_entry);
    char *bit_string;
    ////////////////////////////////////////////////////////////////////////////
    int miss, max_miss = 0, min_miss = num_entry;
    for (j = 0; j < 100; j++) {
        miss = 0;
        for (i = 0; i < num_query; i++) {
            bit_string = convert_to_prefix(query[i].ip, 32);
            begin = rdtsc();
            if (search(bit_string) == 256) {
                miss++;
                printf("Not Found: %x\n", query[i].ip);
            }
            end = rdtsc();
            if (my_clock[i] > (end - begin))
                my_clock[i] = (end - begin);
            free(bit_string);
        }
        // printf("Loss rate: %f\n", (float) miss / (float) num_entry);
        if (miss > max_miss) {
            max_miss = miss;
        }
        if (miss < min_miss)
            min_miss = miss;
    }
    printf("Max Loss rate: %f\n", (float) max_miss / (float) num_entry);
    printf("Min Loss rate: %f\n", (float) min_miss / (float) num_entry);
    total = 0;
    for (j = 0; j < num_query; j++)
        total += my_clock[j];
    num_node = num_query;
    printf("Avg. Search:\t%llu\n", total / num_query);
    printf("Total memory requirement:\t%ld KB\n",
           ((num_entry * sizeof(struct balanced_node)) / 1024));
    CountClock();
    ////////////////////////////////////////////////////////////////////////////
    // count_node(root);
    // printf("There are %d nodes in binary trie\n",N);
    return 0;
}
