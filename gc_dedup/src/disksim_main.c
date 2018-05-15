/*
 * DiskSim Storage Subsystem Simulation Environment (Version 3.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001, 2002, 2003.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */



/*
 * DiskSim Storage Subsystem Simulation Environment (Version 2.0)
 * Revision Authors: Greg Ganger
 * Contributors: Ross Cohen, John Griffin, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 1999.
 *
 * Permission to reproduce, use, and prepare derivative works of
 * this software for internal use is granted provided the copyright
 * and "No Warranty" statements are included with all reproductions
 * and derivative works. This software may also be redistributed
 * without charge provided that the copyright and "No Warranty"
 * statements are included in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 */


#include "disksim_global.h"
//flshsim
#include "ssd_interface.h"


extern int max_laddr;
extern int lhj_max_laddr;


void warmFlashsynth(){

  memset(dm_table, -1, sizeof(int) * DM_MGR_SIZE);

  nand_stat_reset();
  reset_flash_stat();

  if(ftl_type == 3){
    opagemap_reset();
  }

  else if(ftl_type == 4)
  {
    write_count = 0;
    read_count = 0;
  }
}

void warmFlash(char *tname){

  FILE *fp = fopen(tname, "r");
  //char buffer[80];
  char buffer[400];
  double time;
  int devno, bcount, flags;
  long int blkno;
  double delay;
  int i;
  char hash[HASH_LEN];
   int trace_scale;
   
  //trace_scale = 4;
  
  printf("callFsim start\n");
  while(fgets(buffer, sizeof(buffer), fp)){
    sscanf(buffer, "%lf %d %d %d %s\n", &time,  &blkno, &bcount, &flags,hash);

  	
	blkno/=TRACE_SCALE;
  
    bcount = ((blkno + bcount -1) / 4 - (blkno)/4 + 1) * 4;
    blkno /= 4;
    blkno *= 4;
	//blkno%=max_laddr;                        //add by lhj
	blkno %= (max_laddr-4096);   //add by lhg
	blkno+=4000;					//add by lhg
    //flags=0;
  //  printf("callFsim start\n");
    delay = callFsim(hash,blkno, bcount, 0);           //0表示全是写操作
//	printf("hash:%s\n",hash);
//	printf("callFsim end\n");

    for(i = blkno; i<(blkno+bcount); i++){ dm_table[i] = DEV_FLASH; }
  }
  printf("callFsim end\n");
  nand_stat_reset();

 /* if(ftl_type == 3) opagemap_reset(); 

  else if(ftl_type == 4) {
    write_count = 0; read_count = 0; }
*/
  fclose(fp);
}

int read_size(char *str)
	{
		int i=0,j,k=0;
		int flag=0;
		char line[500];
		char sz[20];
		int num=-1;
		FILE *fp;
		fp=fopen(str,"r");
		while(!feof(fp))
		{
			memset(line, '\0', 500);
			fgets(line,500,fp);
			i++;
			if(i==124)
				break;
		}
		//printf("%s\n",line);
		for(j=0;j<strlen(line);j++)
		{
			if(line[j]=='=')
			{
				flag=1;
				continue;
			}
			if(flag == 1&&line[j]!=' ')
			{
				sz[k++]=line[j];
				continue;
			}
	
		}
		num=atoi(sz);
		fclose(fp);
		
		return num;
	}

int main (int argc, char **argv)
{
  int i;
  int len;
  void *addr;
  void *newaddr;
  float ref_1_pec,ref_2_pec,ref_3_pec,ref_4_pec,ref_5_pec,ref_high_pec;
  char warm_file_name[20];     //add by lhj
  rev_root=create_rbtree();  //物理地址为key
  dedup_root=create_rbtree();  //hash为key
  lpn_root = create_lpn_rbtree();
  
  blk_erase=0;
  gc_page_write=0;
 
  max_laddr = read_size(argv[1]);
  printf("max_laddr %d\n",max_laddr);
  if(argc == 2) {
     disksim_restore_from_checkpoint (argv[1]);
  } 
  else {
    len = 8192000 + 2048000 + ALLOCSIZE;//1000 1024
	//len=1024*1024*50;    //add by lhj
    addr = malloc (len);
    newaddr = (void *) (rounduptomult ((long)addr, ALLOCSIZE));
    len -= ALLOCSIZE;

    disksim = disksim_initialize_disksim_structure (newaddr, len);
    disksim_setup_disksim (argc, argv);
  }

  memset(dm_table, -1, sizeof(int)*DM_MGR_SIZE);

  if(ftl_type != -1){

    initFlash();
    reset_flash_stat();
    nand_stat_reset();
  }

//  warmFlashsynth();
// printf("warmFlash start\n");
//strcpy(warm_file_name,"webmail.1-20");
  warmFlash(argv[4]);
//  warmFlash(warm_file_name);   //edit  by lhj
//  printf("warmFlash over\n");
printf("disksim_run_simulation start!\n");
  disksim_run_simulation ();
  printf("disksim_run_simulation end!\n");
  // warmFlash(argv[4]);
  disksim_cleanup_and_printstats ();
  /*
  ref_1_num=ref_2_num=ref_3_num=ref_4_num=ref_5_num=ref_high_num=ref_num=0;
  inorder_rbtree(dedup_root);
  printf("------------------------\n");
  printf("ref_1_num=%d,ref_2_num=%d,ref_3_num=%d,ref_num=%d\n",ref_1_num,ref_2_num,ref_3_num,ref_num);
  printf("ref_4_num=%d,ref_5_num=%d,ref_high_num=%d\n",ref_4_num,ref_5_num,ref_high_num);
  printf("------------------------\n");

	//float ref_1_pec,ref_2_pec,ref_3_pec,ref_4_pec,ref_5_pec,ref_high_pec;
   ref_1_pec=(float)ref_1_num/ref_num;
    ref_2_pec=(float)ref_2_num/ref_num;
    ref_3_pec=(float)ref_3_num/ref_num;
     ref_4_pec=(float)ref_4_num/ref_num;
     ref_5_pec=(float)ref_5_num/ref_num;
     ref_high_pec=(float)ref_high_num/ref_num;
    printf("ref_1_pec=%f,ref_2_pec=%f,ref_3_pec=%f\n",ref_1_pec,ref_2_pec,ref_3_pec);
    printf("ref_4_pec=%f,ref_5_pec=%f,ref_high_pec=%f\n",ref_4_pec,ref_5_pec,ref_high_pec);
    printf("ref_num=%d\n",ref_num);
    */
 // inorder_rbtree(rev_root);
//  printf("write_sum:%d,dedup_sum:%d,dedup_ratio = %lf\n",write_sum,dedup_sum,(write_sum-dedup_sum)*1.0/write_sum);
//  printf("same hash_laddr num:%d\n",same_laddr_hash);
  printf("has run over\n");
 //  printf("GC_page_write=%ld\n",gc_page_write);
//   printf("block erase=%d\n",blk_erase);
 printf("lhj_max_laddr=%d\n",lhj_max_laddr);

  return 0;
}
