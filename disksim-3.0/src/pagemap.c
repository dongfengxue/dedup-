/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu)
 * 
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 *   
 * Description: This source code provides page-level FTL scheme. 
 * 
 * Acknowledgement: We thank Jeong Uk Kang by sharing the initial version 
 * of sector-level FTL source code. 
 * 
 */

#include <stdlib.h>
#include <string.h>
#include<stdio.h>
#include<time.h>

#include "flash.h"
#include "pagemap.h"
#include "type.h"
#include "rbtree.h"


_u32 pm_gc_cost_benefit();

struct map_dir *mapdir;

blk_t extra_blk_num;
_u32 free_blk_no[4];
_u16 free_page_no[4];

int stat_gc_called_num;
double total_gc_overhead_time;

_u32 pm_gc_cost_random()      //随机返回有无效页的blk，edit by lhj
{
  int max_cb = 0;
  int blk_cb;
  int k=0;

  _u32 random_blk = -1, i;
  int valid_blk_no[nand_blk_num];

  for (i = 0; i < nand_blk_num; i++) {

 //   blk_cb = nand_blk[i].ipc;   //无效页计数
	if(nand_blk[i].ipc>0){
		valid_blk_no[k]=i;    //记录无效页大于32的blk num
		k++;
	}
  
  }

  random_blk=valid_blk_no[rand()%k];//生成0-k之间的随机数  

  ASSERT(random_blk != -1);
  ASSERT(nand_blk[random_blk].ipc > 0);
  return random_blk;
}

_u32 pm_gc_cost_benefit()      //返回无效页最多的blk
{
  int max_cb = 0;
  int blk_cb;

  _u32 max_blk = -1, i;

  for (i = 0; i < nand_blk_num; i++) {
   // if(i == free_blk_no[1])                   //相等表示该编号即为空闲blk
   //	if(i==free_blk_no[1]||i==free_blk_no[2])    //add by lhj
//		{ continue; }

    blk_cb = nand_blk[i].ipc;   //无效页计数

    if (blk_cb > max_cb) {
      max_cb = blk_cb;
	  max_blk = i;
    }
  }

  ASSERT(max_blk != -1);
  ASSERT(nand_blk[max_blk].ipc > 0);
//  printf("无效页最多的blk号=%d，最多的无效页计数:%d\n",max_blk,max_cb);  //add by lhj
  return max_blk;
}

size_t pm_read(char hash[],sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE;					
  int size_page = size/SECT_NUM_PER_PAGE;  
  sect_t lsns[SECT_NUM_PER_PAGE];

  sect_t s_lsn;
  sect_t s_psn; 

  int sect_num;

  ASSERT(lpn < pagemap_num);
  ASSERT(lpn + size_page <= pagemap_num);


  memset (lsns, 0xFF, sizeof (lsns));

  sect_num = (size < SECT_NUM_PER_PAGE) ? size : SECT_NUM_PER_PAGE;

  if(mapdir_flag == 2){
    s_psn = mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
  }
  else
  	s_psn = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;

  s_lsn = lpn * SECT_NUM_PER_PAGE;
  
 // printf("pm_read:s_psn=%d,s_lsn=%d\n",s_psn,s_lsn);   //add by lhj
  
  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  size = nand_page_read(s_psn, lsns, 0);

  ASSERT(size == SECT_NUM_PER_PAGE);

  return sect_num;
}

int pm_gc_get_free_blk(int small, int mapdir_flag)
{
  if (free_page_no[small] >= SECT_NUM_PER_BLK) {
    free_blk_no[small] = nand_get_free_blk(1);     //得到gc返回的空闲块号
    free_page_no[small] = 0;

    return -1;
  }
  
  return 0;
}

int pm_gc_get_free_blk_high_ref(int small, int mapdir_flag)
{
  if (free_page_no[small] >= SECT_NUM_PER_BLK) {
    free_blk_no[small] = nand_get_free_blk_high_ref(1);     //得到gc返回的空闲块号
    free_page_no[small] = 0;

    return -1;
  }
  
  return 0;
}

int pm_gc_run(int small, int mapdir_flag)
{
  blk_t victim_blk_no = -1;
  int i, j,m, benefit = 0;
  
  int valid_flag;
  Node * sele_tree_node = NULL;
  struct laddr_list *laddr_l_tmp=NULL;

  _u32 copy_lsn[SECT_NUM_PER_PAGE], copy[SECT_NUM_PER_PAGE];
  _u16 valid_sect_num, k, l, s;
//  printf("*********************************************gc_run start************************************************\n");
//  victim_blk_no = pm_gc_cost_benefit();    //返回无效页最多的blk
  victim_blk_no = pm_gc_cost_random();           //随机返回无效页较多的blk，edit by lhj
//  printf("victim_blk_no:%d\n",victim_blk_no);
  memset(copy_lsn, 0xFF, sizeof (copy_lsn));

  s = k = OFF_F_SECT(free_page_no[small]);             //与11 与运算，得到0-3的数字

  for (i = 0; i < PAGE_NUM_PER_BLK; i++) 
  {
    valid_flag = nand_oob_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE));   //----1代表有效page

    if(valid_flag == 1)       //有效页需要迁移
    {
        valid_sect_num = nand_page_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE), copy, 1);//返回的是有效sec的计数，一般为4

        ASSERT(valid_sect_num == 4);

        k=0;
        for (j = 0; j < valid_sect_num; j++) { copy_lsn[k] = copy[j]; k++; }   //copy[i]=lsn逻辑地址
		
      //-------------------根据冷热small赋值为1,2---------------------//
       sele_tree_node = iterative_rbtree_search_paddr(rev_root,BLK_PAGE_NO_SECT(SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE)));
		//得到ppn
	//	printf("失效blk_ppn=%d,ppn=%d\n",BLK_PAGE_NO_SECT(SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE)),SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE)/4);
		ASSERT(sele_tree_node!=NULL);

     //   #if  REF_FENLI
		if(sele_tree_node!=NULL && (small == 1||small==2||small==3)){  //add by lhj
	//	 #endif
		 	delete_rbtree_paddr(rev_root, sele_tree_node->key->paddr); //------需要需要更改rev_root的索引，删除该失效物理块地址	
		// 	printf("page num in blk:%d，失效的物理块地址：%d\n ",i,sele_tree_node->key->paddr);    //add by lhj
		                
		#if  REF_FENLI
			if(sele_tree_node->key->ref >= THRESHOLD_REF)
				small = 2;           //大于阈值，判断为热
			else
				small = 3;                     //小于阈值，判断为冷
				
		#endif
		}
		//  if(free_blk_no[small]==-2)
  		//	free_blk_no[small] =nand_get_free_blk(0);
	//	if(small==2)
        //	benefit += pm_gc_get_free_blk_high_ref(small, mapdir_flag);   //迁移后的数据放置的blk
    //    else
			benefit += pm_gc_get_free_blk(small, mapdir_flag);
		//------此处需要根据逻辑地址修改--------//
        pagemap[BLK_PAGE_NO_SECT(copy_lsn[s])].ppn = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small]));
	//	printf("新的ppn地址为%d\n",BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])));
		
        laddr_l_tmp = sele_tree_node->key->laddr_l;
        while(laddr_l_tmp){
			pagemap[laddr_l_tmp->laddr].ppn = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])); //该逻辑地址对应的新的有效物理地址
			laddr_l_tmp = laddr_l_tmp->next;
		}	
		sele_tree_node->key->paddr = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small]));
		insert_rbtree_paddr(rev_root, sele_tree_node->key);  //------需要需要更改rev_root的索引	
		//------此处需要根据逻辑地址修改---xia-----//
		
        nand_page_write(SECTOR(free_blk_no[small],free_page_no[small]) & (~OFF_MASK_SECT), copy_lsn, 1, 1);

		gc_page_write++;    //add by lhj
	//	printf("gc_page_write:%d\n",gc_page_write);
		
        free_page_no[small] += SECT_NUM_PER_PAGE;
		
		 //-------------------根据冷热small赋值为1,2-------xia--------------//
    }
  }

//  printf("nand_erase\n");    
 // if(small==1||small==2||small==3)
 // 	small=1;
  nand_erase(victim_blk_no);

 // blk_erase++;
//  printf("gc_run end\n");
  return (benefit + 1);
}

size_t pm_write(char hash[],sect_t lsn, sect_t size, int mapdir_flag)  
{
  //-----------去重后return 0；
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE;					
  int size_page = size/SECT_NUM_PER_PAGE;   	
  int ppn;
  int small;
  int sect_num;
  struct data_node *data_tmp=NULL;
  Node *tree_node_tmp=NULL;
  Node *dele_tree_node=NULL;
  int dele_paddr;
  sect_t s_lsn;	
  sect_t s_psn; 
  sect_t s_psn1;
  sect_t lsns[SECT_NUM_PER_PAGE];
  static int w_num=0;

  w_num++;
  write_sum++;
 // printf("in pm_write w_num is %d\n",w_num);
 // printf("lsn:%d,size: %d,hash: %s\n",lsn,size,hash);
 if(lpn>pagemap_num){
  printf("lpn :%d, page_num: %d\n",lpn,pagemap_num);
 	}
  ASSERT(lpn < pagemap_num);
  ASSERT(lpn + size_page <= pagemap_num);

  s_lsn = lpn * SECT_NUM_PER_PAGE;

  small = 1;
    compute_hash();       //计算hash次数，每次计算后重置
  //--------------在此去重-----------------//
  tree_node_tmp=iterative_rbtree_search_hash(dedup_root, hash);  //hash 为key的tree
  if(tree_node_tmp!=NULL){             //不为空表示，有相同的hash已经在节点中
  	//printf("tree_node_tmp!=NULL,有相同的hash在节点中\n");
  		  if(search_laddr_in_node(tree_node_tmp->key, lpn))  //逻辑地址一样，找到返回4
		  	{ 
		  	same_laddr_hash++;
		//	printf("逻辑地址和hash都一样的个数：%d\n",same_laddr_hash);
		  	return 4;
  		  	}

		//    printf("插入，ref++");
    	   tree_node_tmp->key->ref++;						//hash相同，逻辑地址不同，ref++
		   insert_laddr_in_node(tree_node_tmp->key,lpn);            //将该逻辑地址加入到相应节点
		   ppn=tree_node_tmp->key->paddr;
		   
		//   printf("hash相同，逻辑地址不同的：ref=%d,lpn=%d,ppn=%d,hash=%s\n",tree_node_tmp->key->ref,lpn,ppn,tree_node_tmp->key->md);
		   
		   //----------覆盖检测---------------------//
		  if (pagemap[lpn].free == 0) {          //该逻辑地址对应的物理地址不可用
		  	 //  printf("覆盖检测 1\n");
		  	   dele_paddr = pagemap[lpn].ppn;
		  //	   printf("已经失效的ppn地址：lpn=%d,ppn=%d\n",lpn,dele_paddr);
			
			   dele_tree_node=iterative_rbtree_search_paddr(rev_root, dele_paddr);
		       if(dele_tree_node){
				  dele_tree_node->key->ref--;	
				//  printf("删除地址的ref：ref=%d\n", dele_tree_node->key->ref);
			   
				  if(dele_tree_node->key->ref<=0){
				  	//	printf("删除的节点信息：paddr=%d,hash=%s \n",dele_tree_node->key->paddr,dele_tree_node->key->md);
						delete_data_node(dele_tree_node->key, dedup_root, rev_root);
						
				  
						s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;		       
			  			for(i = 0; i<SECT_NUM_PER_PAGE; i++){
				 		 nand_invalidate(s_psn1 + i, s_lsn + i);                           //可能有问题
						} 
			  		    nand_stat(3);    //3 is oob_write
				  }
				  else{
				//  	printf("覆盖检测 2\n");
					delete_laddr_in_data_node(dele_tree_node->key, lpn);//-------删除对应的laddr
					//if(dele_tree_node->key->ref==1){
						s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;
				//	printf(" 覆盖删除set nand_blk_lpn\n");
						for(i = 0; i<SECT_NUM_PER_PAGE; i++){
								set_nand_blk_lpn(s_psn1 + i, dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE+i); //-----------很重要
						//set_nand_blk_lpn(s_psn1 , dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE);
						}
					//	}
						
				 	}
			   }
			   
		 }
		else {
			   pagemap[lpn].free = 0;         //该lpn对应的ppn为非free状态，即有数据
			 }
		//----------覆盖检测---------xia------------//
		 if(mapdir_flag == 2) {
    		mapdir[lpn].ppn = ppn;
    		pagemap[lpn].ppn = ppn;
  		 }
		 else {
    		pagemap[lpn].ppn = ppn;
  			}  
  return 4;
}
  //-------------在此去重-----xia------------//
//     原先的GC策略
  if (free_page_no[small] >= SECT_NUM_PER_BLK)   //段内的sector地址偏移大于段内总地址，说明段内地址用光，需要一个新的blk
  {
    if ((free_blk_no[small] = nand_get_free_blk(0)) == -1)  //if为真的条件是，没有free blk,free blk <min_free_blk_num
    {
      int j = 0;

	 // printf("free_blk_num:%d\n",free_blk_num);
	 if(free_blk_num<min_free_blk_num){    //edit by lhug
	 	while (free_blk_num < min_free_blk_num+min_free_blk_num/8){ //每次gc1个段,效果最好
	 // 	while (free_blk_num < 4){
	  //	printf("*********************************************gc_run start************************************************\n");
	 // 		small=1;   //add by lhj
        j += pm_gc_run(small, mapdir_flag);       //调用触发gc
      }
	 }
      
      pm_gc_get_free_blk(small, mapdir_flag);
    } 
    else {
      free_page_no[small] = 0;       //否认从页开头
    }
  }


//*************************************************按照一定写入量修改的GC策略*************************************************//
/*
if(free_page_no[small]>=SECT_NUM_PER_BLK)
{
	if(disk_write>DISK_GC_THRESHHOLD)    //写入量大于gc阈值
	{
		int j=0;
		while(j<512){     //一次gc 512个块
			j+=pm_gc_run(small,mapdir_flag);                      //调用gc函数		
		}
		pm_gc_get_free_blk(small,mapdir_flag);
	}

}
*/
  memset (lsns, 0xFF, sizeof (lsns));
  sect_num = SECT_NUM_PER_PAGE;
  
  s_psn = SECTOR(free_blk_no[small], free_page_no[small]);

  if(s_psn % 4 != 0){
    printf("s_psn: %d\n", s_psn);
  }
  ASSERT(s_psn%4==0);
  
  ppn = s_psn / SECT_NUM_PER_PAGE;
 // printf("ppn=%d\n",ppn);
  //----------非重复处理-------------------------//
 // printf("非重复处理 ，插入新的hash节点\n");
   dedup_sum++;							  //记录第一次插入的次数
  data_tmp = create_data_node(ppn,lpn, 1, hash);
  insert_rbtree_hash(dedup_root, data_tmp);
  insert_rbtree_paddr(rev_root, data_tmp); 
//  printf("非重复处理 2\n");
  //----------非重复处理-------xia------------------//
  if (pagemap[lpn].free == 0) {                     //该lpn对应的物理地址无效
  	         //----------ref--&节点删除-------------//
   			  dele_paddr = pagemap[lpn].ppn;
			 
			// printf("dele_paddr:%d\n",dele_paddr);    //add by lhj
  
			  dele_tree_node=iterative_rbtree_search_paddr(rev_root, dele_paddr);
  			   ASSERT(dele_tree_node!=NULL);
			   
		       if(dele_tree_node){
				  dele_tree_node->key->ref--;
				  
				  if(dele_tree_node->key->ref<=0){        //引用值小于等于0，删除该节点
				 // 	printf("删除data_node节点 start\n");
					delete_data_node(dele_tree_node->key, dedup_root, rev_root);
				 // 	printf("删除data_node节点end\n");
			  		//----------ref--&节点删除-----xia--------//    			  
					s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;		       
			  		for(i = 0; i<SECT_NUM_PER_PAGE; i++){ 
				 		nand_invalidate(s_psn1 + i, s_lsn + i);                            //可能有问题
			  		 } 
			  		 nand_stat(3);      //3 is OOB_WRiTE
				 }
				  
				 else{
				 //	printf("delete_laddr_in_data_node laddr %d\n",lpn);
					delete_laddr_in_data_node(dele_tree_node->key, lpn);//-------删除对应的laddr
					//if(dele_tree_node->key->ref==1){
						s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;	
				//	printf("del set nand_blk_lpn\n");
			  			 for(i = 0; i<SECT_NUM_PER_PAGE; i++){ 
				 				set_nand_blk_lpn(s_psn1 + i, dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE+i);
					//	set_nand_blk_lpn(s_psn1 , dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE);
			  			 } 
					//}
					
				 }
			   }		
  }
  else {
    pagemap[lpn].free = 0;
  }

  for (i = 0; i < SECT_NUM_PER_PAGE; i++) 
  {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2) {
    mapdir[lpn].ppn = ppn;
    pagemap[lpn].ppn = ppn;
  }
  else {
    pagemap[lpn].ppn = ppn;
  }

  free_page_no[small] += SECT_NUM_PER_PAGE;
  nand_page_write(s_psn, lsns, 0, mapdir_flag);

  return sect_num;
}

void pm_end()
{
  if (pagemap != NULL) {
    free(pagemap);
    free(mapdir);
  }
  pagemap_num = 0;
}

void pagemap_reset()
{
  cache_hit = 0;
  flash_hit = 0;
  disk_hit = 0;
  evict = 0;
  delay_flash_update = 0; 
}

int pm_init(blk_t blk_num, blk_t extra_num)
{
  int i;
  int mapdir_num;

  printf("blk_num=%d\n",blk_num);
  pagemap_num = blk_num * PAGE_NUM_PER_BLK;

  pagemap = (struct pm_entry *) malloc(sizeof (struct pm_entry) * pagemap_num);
  mapdir = (struct map_dir *)malloc(sizeof(struct map_dir) * pagemap_num / MAP_ENTRIES_PER_PAGE); 

  if ((pagemap == NULL) || (mapdir == NULL)) {
    return -1;
  }

  mapdir_num = (pagemap_num / MAP_ENTRIES_PER_PAGE);

  if((pagemap_num % MAP_ENTRIES_PER_PAGE) != 0){
    printf("pagemap_num % MAP_ENTRIES_PER_PAGE is not zero\n"); 
    mapdir_num++;
  }

  memset(pagemap, 0xFF, sizeof (struct pm_entry) * pagemap_num);
  memset(mapdir,  0xFF, sizeof (struct map_dir) * mapdir_num);

  TOTAL_MAP_ENTRIES = pagemap_num;

  for(i = 0; i<TOTAL_MAP_ENTRIES; i++){
    pagemap[i].cache_status = 0;
    pagemap[i].cache_age = 0;
    pagemap[i].map_status = 0;
    pagemap[i].map_age = 0;
  }

  extra_blk_num = extra_num;

  free_blk_no[1] = nand_get_free_blk(0);	//写入的数据段
  free_page_no[1] = 0;
  free_blk_no[2] = nand_get_free_blk(0);  //----强数据段
  free_page_no[2] = 0;
 free_blk_no[3] =nand_get_free_blk(0);		//----冷数据段
 free_page_no[3]=0;

  MAP_REAL_NUM_ENTRIES = 0;
  MAP_GHOST_NUM_ENTRIES = 0;
  CACHE_NUM_ENTRIES = 0;
  SYNC_NUM = 0;

  cache_hit = 0;
  flash_hit = 0;
  disk_hit = 0;
  evict = 0;
  read_cache_hit = 0;
  write_cache_hit = 0;

  /*
  for(i = 0; i<(pagemap_num); i++){
    pm_write(i*SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 1);
  }
  */

  return 0;
}

struct ftl_operation pm_operation = {
  init:  pm_init,
  read:  pm_read,
  write: pm_write,
  end:   pm_end
};
  
struct ftl_operation * pm_setup()
{
  return &pm_operation;
}
