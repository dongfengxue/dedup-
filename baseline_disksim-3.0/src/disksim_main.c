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

void warmFlash(char *tname){

  FILE *fp = fopen(tname, "r");
  char buffer[400];
  double time;
 // int devno, bcount, flags;
  int bcount,flags;
  long int blkno;
  double delay;
  char *process; //add by lhj,进程号，可不用
  int devno;
  char hash[257];
  int i;
  int trace_scale;

  while(fgets(buffer, sizeof(buffer), fp)){
   sscanf(buffer, "%lf %d %d %d %s\n", &time,  &blkno, &bcount, &flags,hash);
	//sscanf(buffer, "%lf %d %d %d  %s %s %s\n", &time, &flags, &blkno, &bcount, &process,&devno,&hash);
    //   devno=0;
   // bcount = ((blkno + bcount -1) / 4 - (blkno)/4 + 1) * 4;

	// trace_scale = 4;
	  blkno/=TRACE_SCALE;
	bcount = ((blkno + bcount -1) / 4 - (blkno)/4 + 1) * 4;
    blkno /= 4;
    blkno *= 4;
    blkno %= (max_laddr-4096);
	blkno+=4000;
    delay = callFsim(blkno, bcount, 0);   

    for(i = blkno; i<(blkno+bcount); i++){ dm_table[i] = DEV_FLASH; }
  }
  nand_stat_reset();

  if(ftl_type == 3) opagemap_reset(); 

  else if(ftl_type == 4) {
    write_count = 0; read_count = 0; }

  fclose(fp);
}

int main (int argc, char **argv)
{
  int i;
  int len;
  void *addr;
  void *newaddr;

   max_laddr = read_size(argv[1]);
 //  lpn_root = create_lpn_rbtree();
   
  if(argc == 2) {
     disksim_restore_from_checkpoint (argv[1]);
  } 
  else {
    len = 8192000 + 2048000 + ALLOCSIZE;     //(8000+2000+8)*1024
    addr = malloc (len);
    newaddr = (void *) (rounduptomult ((long)addr, ALLOCSIZE));
    len -= ALLOCSIZE;

    disksim = disksim_initialize_disksim_structure (newaddr, len);         //初始化模拟器结构
    disksim_setup_disksim (argc, argv);         //初始化disksim，读取配置文件，trace文件等
  }

  memset(dm_table, -1, sizeof(int)*DM_MGR_SIZE);

  if(ftl_type != -1){

    initFlash();                 		//初始化闪存
    reset_flash_stat();                 //flash 状态初始化
    nand_stat_reset();
  }

 // warmFlashsynth();
 printf("warm_up start!\n");
  warmFlash(argv[4]);
printf("warm_up end!\n");
printf("disksim_run_simulation开始！\n");
  disksim_run_simulation ();
  printf("disksim_run_simulation 结束！\n");

  disksim_cleanup_and_printstats ();

  printf("lhj_max_laddr=%d\n",lhj_max_laddr);

  return 0;
}
