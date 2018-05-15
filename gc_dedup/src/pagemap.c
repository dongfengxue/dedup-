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

static int last_write;
_u32 pm_gc_cost_random()      //随机返回有无效页的blk，edit by lhj
{
  int max_cb = 0;
  int blk_cb;
  int k=0;

  _u32 random_blk = -1, i;
  int invalid_blk_no[nand_blk_num];

  for (i = 0; i < nand_blk_num; i++) {

 //   blk_cb = nand_blk[i].ipc;   //无效页计数
	if(nand_blk[i].ipc>0 && nand_blk[i].blk_ref<128){
		invalid_blk_no[k]=i;    //记录无效页大于32的blk num
		k++;
	}
  
  }

  random_blk=invalid_blk_no[rand()%k];//生成0-k之间的随机数  

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
 // printf("无效页最多的blk号=%d，最多的无效页计数:%d\n",max_blk,max_cb);  //add by lhj
  return max_blk;
}

size_t pm_read(char hash[],sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
 // int lpn;
  int lpn = lsn/SECT_NUM_PER_PAGE;					
  int size_page = size/SECT_NUM_PER_PAGE;  
  sect_t lsns[SECT_NUM_PER_PAGE];

  sect_t s_lsn;
  sect_t s_psn; 

  int sect_num;

//	lsn = last_write; //-lhg
//	lpn = lsn/SECT_NUM_PER_PAGE;

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

int pm_gc_get_free_blk_high_ref(int small, int mapdir_flag)     //add by lhj
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

  struct data_node *data_tmp=NULL;
  Node *tree_node_paddr=NULL;
  Node *tree_node_tmp=NULL;
  Node *dele_tree_node=NULL;
  int dele_paddr;
  sect_t s_psn1;
  int ref_count;


  _u32 psn,ppn,lpn,lsn;
  blk_t pbn;
  _u16 pin;
  char hash[257];
  _u32 pbn1 ;    //统计该pbn的ref
   
  int valid_flag;
  Node * sele_tree_node = NULL;
  struct laddr_list *laddr_l_tmp=NULL;

  _u32 copy_lsn[SECT_NUM_PER_PAGE], copy[SECT_NUM_PER_PAGE];
  _u16 valid_sect_num, k, l, s;
//  printf("*********************************************gc_run start************************************************\n");
 // victim_blk_no = pm_gc_cost_benefit();    //返回无效页最多的blk
  victim_blk_no = pm_gc_cost_random();           //随机返回无效页较多的blk，edit by lhj
  memset(copy_lsn, 0xFF, sizeof (copy_lsn));

  s = k = OFF_F_SECT(free_page_no[small]);             //与11 与运算，得到0-3的数字

  for (i = 0; i < PAGE_NUM_PER_BLK; i++) 
  {
    valid_flag = nand_oob_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE));   //----1代表有效page

    if(valid_flag == 1)       //有效页需要迁移
    {
    //	printf("有效页迁移\n");
        valid_sect_num = nand_page_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE), copy, 1);//返回的是有效sec的计数，一般为4
			//copy [i]中存放的是nand_blk[pbn].sect[pin + i].lsn
        ASSERT(valid_sect_num == 4);

        k=0;
        for (j = 0; j < valid_sect_num; j++) { copy_lsn[k] = copy[j]; k++; }   //copy[i]=lsn逻辑地址
		//------------------------gc 重删--------------------------------//
		psn = SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE );   //迁移页的物理sect号
		ppn = BLK_PAGE_NO_SECT(psn);            //迁移页的物理页号
		pbn = BLK_F_SECT(psn);
		pin = IND_F_SECT(psn);
		
	
		lpn= nand_blk[pbn].sect[pin].lsn / SECT_NUM_PER_PAGE;


		ASSERT(lpn==BLK_PAGE_NO_SECT(copy_lsn[s]));    //两种算法一致性
		strcpy(hash,pagemap[lpn].hash);

	//	printf("ppn=%d,pagemap[lpn].ppn=%d\n",ppn,pagemap[lpn].ppn);
		ASSERT(pagemap[lpn].ppn==ppn);     //映射关系正确

		compute_hash();
		//-----------------判断该ppn对应的有效页是否重复,在此去重-----------//
		tree_node_tmp = iterative_rbtree_search_hash(dedup_root,hash);
		tree_node_paddr=tree_node_tmp;
		if(tree_node_tmp!=NULL){  //不为空表示，迁移过相同hash的数据 
			
		sele_tree_node = iterative_rbtree_search_paddr(rev_root,ppn);
				if(ppn != tree_node_tmp->key->paddr){      
				
					tree_node_tmp->key->ref++;   //是相同的hash数据,不一样的lpn
					insert_laddr_in_node(tree_node_tmp->key, lpn);   //保存迁移后的lpn
					pagemap[lpn].ppn=tree_node_tmp->key->paddr;  //该lpn新对应的ppn
					//strcpy(pagemap[lpn].hash,tree_node_tmp->key->md);   //该lpn新对应的hash值

					s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;		 //原来lpn对应的ppn无效     
					pbn1= BLK_F_SECT(s_psn1); 
					nand_blk[pbn1].blk_ref++;
						
					ASSERT(strcmp(pagemap[lpn].hash,tree_node_tmp->key->md)==0);
					
				}
				else{    //这个ppn之前迁移过
					ASSERT(sele_tree_node!=NULL);
				     ASSERT(search_laddr_in_node(tree_node_tmp->key, lpn)==1);
					// ASSERT(search_paddr(tree_node_tmp, ppn)!=NULL);
					 ASSERT(strcmp(sele_tree_node->key->md,tree_node_tmp->key->md)==0);
					 delete_rbtree_paddr(rev_root,tree_node_tmp->key->paddr);
					#if  REF_FENLI
					if(tree_node_tmp->key->ref >= THRESHOLD_REF)
						small = 2;           //大于阈值，判断为热
					else
						small = 3;          //小于阈值，判断为冷
						
					#endif
					ref_count=tree_node_tmp->key->ref;
				//	if(small==2)
					//	benefit += pm_gc_get_free_blk_high_ref(small, mapdir_flag);	 //迁移后的数据放置的blk	
					//else
						benefit += pm_gc_get_free_blk(small, mapdir_flag);	 //迁移后的数据放置的blk
					pagemap[BLK_PAGE_NO_SECT(copy_lsn[s])].ppn = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small]));
       		
					laddr_l_tmp = tree_node_tmp->key->laddr_l;
    	   			 while(laddr_l_tmp){     //所有该原先节点包含的都需要修改
						pagemap[laddr_l_tmp->laddr].ppn = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])); //该逻辑地址对应的新的有效物理地址
						laddr_l_tmp = laddr_l_tmp->next;
					}
			
					  tree_node_tmp->key->paddr=BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small]));	
					  tree_node_tmp->key->ref=ref_count;
					  insert_rbtree_paddr(rev_root, tree_node_tmp->key);
					  
				//	 printf("插入新节点的ref=%d,small=%d\n",tree_node_paddr->key->ref,small);
					 
					 
					 nand_page_write(SECTOR(free_blk_no[small],free_page_no[small]) & (~OFF_MASK_SECT), copy_lsn, 1, 1);  //将该ppn映射到lpn
					 free_page_no[small] += SECT_NUM_PER_PAGE;

					s_psn1 =BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])) * SECT_NUM_PER_PAGE;		 //原来lpn对应的ppn无效     
					pbn1= BLK_F_SECT(s_psn1); 
					nand_blk[pbn1].blk_ref++;
				}
			
		}

		else{     //否则是第一次迁移该hash数据
		//compute_hash();
		ASSERT(tree_node_tmp==NULL);
        benefit += pm_gc_get_free_blk(small, mapdir_flag);   //迁移后的数据放置的blk
		//------此处需要根据逻辑地址修改--------//
		ASSERT(lpn==BLK_PAGE_NO_SECT(copy_lsn[s]));
        pagemap[BLK_PAGE_NO_SECT(copy_lsn[s])].ppn = BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small]));  //迁移后的ppn

		//第一次GC该hash值数据
		ASSERT(pagemap[lpn].ppn==BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])));
		data_tmp = create_data_node(pagemap[lpn].ppn,lpn, 1,hash);   //gc迁移后的lpn，ppn
		insert_rbtree_hash(dedup_root,data_tmp);
		insert_rbtree_paddr(rev_root, data_tmp);

		//compute_hash();
        nand_page_write(SECTOR(free_blk_no[small],free_page_no[small]) & (~OFF_MASK_SECT), copy_lsn, 1, 1);  //将该ppn映射到lpn

		gc_page_write++;    //add by lhj
	//	printf("gc_page_write:%d\n",gc_page_write);
        free_page_no[small] += SECT_NUM_PER_PAGE;

		s_psn1 =BLK_PAGE_NO_SECT(SECTOR(free_blk_no[small], free_page_no[small])) * SECT_NUM_PER_PAGE;		 //原来lpn对应的ppn无效     
		pbn1= BLK_F_SECT(s_psn1); 
	    nand_blk[pbn1].blk_ref++;
	
		}
		
		 //-------------------根据冷热small赋值为1,2-------xia--------------//
    }
  }


  nand_erase(victim_blk_no);


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
  _u32 pbn ;
   int flag=-1;
  w_num++;
  write_sum++;
   //last_write=lsn; //-lhg
 // printf("in pm_write w_num is %d\n",w_num);
 // printf("lsn:%d,size: %d,hash: %s\n",lsn,size,hash);
 if(lpn>pagemap_num){
  printf("lpn :%d, page_num: %d\n",lpn,pagemap_num);
 	}
  ASSERT(lpn < pagemap_num);
  ASSERT(lpn + size_page <= pagemap_num);

  s_lsn = lpn * SECT_NUM_PER_PAGE;

  small = 1;
 
//     原先的GC策略
  if (free_page_no[small] >= SECT_NUM_PER_BLK)   //段内的sector地址偏移大于段内总地址，说明段内地址用光，需要一个新的blk
  {
    if ((free_blk_no[small] = nand_get_free_blk(0)) == -1)  //if为真的条件是，没有free blk,free blk <min_free_blk_num
    {
      int j = 0;

	 // printf("free_blk_num:%d\n",free_blk_num);
	// printf("触发gc，start\n");
	 if(free_blk_num<min_free_blk_num){    //edit by lhug
	 	while (free_blk_num < min_free_blk_num+total_util_blk_num*0.1){    //每次gc25%,效果最好
      	  j += pm_gc_run(small, mapdir_flag);       //调用触发gc
    	  }
		gc_count++;
	 }
	 printf("gc 次数=%d\n",gc_count);
	//  fprintf (outputfile, "gc次数=%d \n", gc_count);
	//  printf("gc完成，end\n");
      
      pm_gc_get_free_blk(small, mapdir_flag);
    } 
    else {
      free_page_no[small] = 0;       //否则从页开头
    }
  }

  memset (lsns, 0xFF, sizeof (lsns));
  sect_num = SECT_NUM_PER_PAGE;

  strcpy(pagemap[lpn].hash,hash);   //add by lhj,多个lpn对应一个hash

  //覆盖检测
  if (pagemap[lpn].free == 0){     //有数据

   dele_paddr = pagemap[lpn].ppn;		
   dele_tree_node=iterative_rbtree_search_paddr(rev_root, dele_paddr);  //查找该物理地址
   if(dele_tree_node){    //如果该物理地址存在
   		ASSERT(search_laddr_in_node(dele_tree_node->key, lpn)==1);
		//printf("覆盖检测！\n");
   	    flag= delete_laddr_in_data_node(dele_tree_node->key, lpn);  //-------删除对应的laddr
   	    	ASSERT(flag==1);
	        if(flag==1){
                   dele_tree_node->key->ref--;	
				   ASSERT(dele_tree_node->key->ref>=0);
				 if(dele_tree_node->key->ref<=0){
					// 	printf("覆盖检测！删除节点\n");
				  	//	printf("删除的节点信息：paddr=%d,hash=%s \n",dele_tree_node->key->paddr,dele_tree_node->key->md);
						delete_data_node(dele_tree_node->key, dedup_root, rev_root);
						s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;		 //原来lpn对应的ppn无效     
						
						pbn= BLK_F_SECT(s_psn1); 
						nand_blk[pbn].blk_ref--;
						
			  			for(i = 0; i<SECT_NUM_PER_PAGE; i++){
				 		 nand_invalidate(s_psn1 + i, s_lsn + i);                           //可能有问题
						} 
			  		    nand_stat(3);    //3 is oob_write
			  		//    return 4;
				  	}
				  else{
				 // 	printf("覆盖检测！置位ppn对应的lsn\n");
					s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;
				 
						 pbn= BLK_F_SECT(s_psn1); 
						nand_blk[pbn].blk_ref--;
				
					for(i = 0; i<SECT_NUM_PER_PAGE; i++){
						set_nand_blk_lpn(s_psn1 + i, dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE+i); //-----------很重要
						//set_nand_blk_lpn(s_psn1 , dele_tree_node->key->laddr_l->laddr*SECT_NUM_PER_PAGE);
					}	
				 }
				  
			 }
  	}
   else{
   		ASSERT(dele_tree_node==NULL);
   		 s_psn1 = pagemap[lpn].ppn * SECT_NUM_PER_PAGE;

		  pbn= BLK_F_SECT(s_psn1); 
		  nand_blk[pbn].blk_ref--;
						
		for(i = 0; i<SECT_NUM_PER_PAGE; i++){
		  nand_invalidate(s_psn1 + i, s_lsn + i);		  //相应物理section页面置无效，置四次，page无效
		} 
		nand_stat(3);
   	}
}  
  else {
	  pagemap[lpn].free = 0;         //该lpn对应的ppn为非free状态，即有数据
 }



	 s_psn = SECTOR(free_blk_no[small], free_page_no[small]);     //迁移后的psn

	  if(s_psn % 4 != 0){
	    printf("s_psn: %d\n", s_psn);
	  }
	  ASSERT(s_psn%4==0);
	  ppn = s_psn / SECT_NUM_PER_PAGE;	  //新写入的ppn地址


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
	  
	  nand_page_write(s_psn, lsns, 0, mapdir_flag);     //psn 对应的lpn
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
