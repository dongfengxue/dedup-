//
// Created by lhg on 2018/4/17.
//

#ifndef TEST_LPN_RBTREE_LPN_RBTREE_H
#define TEST_LPN_RBTREE_LPN_RBTREE_H

#define RED        0    // 红色节点
#define BLACK    1    // 黑色节点


struct ftl_l_p{                                 //-----------------debug2
    int laddr;
    int curr_laddr;
};

struct lpn_tree_node{                                       //-----------------debug2
    unsigned char color;		// 颜色(RED 或 BLACK)
    struct ftl_l_p  *key; 				   // 关键字(键值)
    struct lpn_tree_node *left;	// 左孩子
    struct lpn_tree_node *right;	 // 右孩子
    struct lpn_tree_node *parent;	  // 父结点
};
struct lpn_rb_root{          //--- -------------debug2
    struct lpn_tree_node *node;
};


struct lpn_rb_root* create_lpn_rbtree(); //-----------------debug2
// 销毁红黑树

void destroy_lpn_rbtree(struct lpn_rb_root *root);
void midorder_lpn_rbtree(struct lpn_rb_root *root);


// 将结点插入到红黑树中。插入成功，返回0；失败返回-1。
int insert_lpn_rbtree_lpn(struct lpn_rb_root *root,struct ftl_l_p *key);
// 删除结点(key为节点的值)
void delete_lpn_rbtree_lpn(struct lpn_rb_root *root, int key);


// (非递归实现)查找"红黑树"中键值为key的节点。找到的话，返回0；否则，返回-1。

struct lpn_tree_node * iterative_lpn_rbtree_search_lpn(struct lpn_rb_root *root, int key);
struct ftl_l_p *create_ftl_l_p_node(int cur_laddr,int lpn);
void delete_ftl_l_p_node(struct  ftl_l_p *ftl_tmp, struct lpn_rb_root *root);
struct lpn_rb_root *lpn_root;


//RBRoot *cache_root;


#endif //TEST_LPN_RBTREE_LPN_RBTREE_H

