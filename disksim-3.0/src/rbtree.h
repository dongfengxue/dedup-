#ifndef _RED_BLACK_TREE_H_
#define _RED_BLACK_TREE_H_
#define COVER_TEST     0
#define DEBUG_RBTREE   1
#define DEBUG_PRINTF   0
#define READ_UNTEST    1

#define RED        0    // 红色节点
#define BLACK    1    // 黑色节点

#include "type.h"

struct laddr_list{
    struct laddr_list *next;
    int laddr;
};
struct data_node{
  //  char md[33];
  	char md[257];   //适应homes的hash长度
    int ref;
    int paddr;
    struct laddr_list *laddr_l;
};


// 红黑树的节点
typedef struct RBTreeNode{
    unsigned char color;        // 颜色(RED 或 BLACK)
    struct data_node  *key;                    // 关键字(键值)
    struct RBTreeNode *left;    // 左孩子
    struct RBTreeNode *right;    // 右孩子
    struct RBTreeNode *parent;    // 父结点
}Node, *RBTree;

// 红黑树的根
typedef struct rb_root{
    Node *node;
}RBRoot;

// 创建红黑树，返回"红黑树的根"！
RBRoot* create_rbtree();

// 销毁红黑树
void destroy_rbtree(RBRoot *root);

// 将结点插入到红黑树中。插入成功，返回0；失败返回-1。
int insert_rbtree_paddr(RBRoot *root, struct data_node *key);
int insert_rbtree_hash(RBRoot *root, struct data_node *key);
int insert_rbtree_laddr(RBRoot *root, struct data_node *key);
// 删除结点(key为节点的值)
void delete_rbtree_paddr(RBRoot *root, int key);
void delete_rbtree_hash(RBRoot *root, char hash[]);
void delete_rbtree_laddr(RBRoot *root, int key);
// 前序遍历"红黑树"
void preorder_rbtree(RBRoot *root);
// 中序遍历"红黑树"
void inorder_rbtree(RBRoot *root);
// 后序遍历"红黑树"
void postorder_rbtree(RBRoot *root);

// (递归实现)查找"红黑树"中键值为key的节点。找到的话，返回0；否则，返回-1。
int rbtree_search_paddr(RBRoot *root, int key);
// (非递归实现)查找"红黑树"中键值为key的节点。找到的话，返回0；否则，返回-1。
Node * iterative_rbtree_search_paddr(RBRoot *root, int key);
Node * iterative_rbtree_search_hash(RBRoot *root, char hash[]);
Node * iterative_rbtree_search_laddr(RBRoot *root, int key);

// 返回最小结点的值(将值保存到val中)。找到的话，返回0；否则返回-1。
int rbtree_minimum_paddr(RBRoot *root, int *val);
// 返回最大结点的值(将值保存到val中)。找到的话，返回0；否则返回-1。
int rbtree_maximum_paddr(RBRoot *root, int *val);

// 打印红黑树
void print_rbtree(RBRoot *root);

struct data_node *create_data_node(int paddr,int laddr,int ref,char md[]);

void delete_data_node(struct data_node *data_tmp, RBRoot *dedup_root, RBRoot *rev_root);
void insert_laddr_in_node(struct data_node *data_tmp,int laddr);
int search_laddr_in_node(struct data_node *data_tmp,int laddr);
void see_laddr_in_node(struct data_node *data_tmp);


RBRoot *dedup_root;
RBRoot *rev_root;
#define  THRESHOLD_REF 3			//分离的ref阈值
#define REF_FENLI  0            //是否做分离处理
int write_sum;          //add by lhg  统计写入的次数
int dedup_sum;			//add by lhg

int same_laddr_hash;
_u32 gc_page_write;
_u32 blk_erase;
//RBRoot *cache_root;

//add by lhj
int ref_1_num;      //ref等于1的个数
int ref_2_num;
int ref_3_num;
int ref_4_num;
int ref_5_num;
int ref_high_num;   //ref大于5的个数
int ref_num;      //节点计数
#endif

