﻿/*
 * AUTHOR: Sergey Erokhin                 esl@pisem.net,pk8020@gmail.com
 * &Korvet Team                                              2000...2005
 * ETALON Korvet Emulator                         http://pk8020.narod.ru
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */
#include "korvet.h"
#include "ext_rom.h"

// Поддержка загрузки с внешнего ROM

int   ext_rom_mode=0;
FILE* extrom_file;       		
char  ext_rom_file_name[1024]="not set";	 // имя файла с образом ROM
int   ext_rom_addr_changed=0;              // =1 while EXT ROM BOOT (rom loader only write in PPI3B,PPI3C)
char  ext_rom_emu_folder[1024]="";         // папка которая прикидывается SDCARD эмулятора

int   e_cmd=0;
int   e_trk=0;
int   e_sec=0;
int   e_drv=0;
int   e_crc=0;

int   e_substitute_system_track=0;

#define EXT_BUF_SIZE (64*1024)
byte in_buffer[EXT_BUF_SIZE];
int in_buffer_size=0;

byte out_buffer[EXT_BUF_SIZE];
int out_buffer_size=0;

int emu_stage=EMU_STAGE1;

FILE *f_emu=NULL;

char tmp_path[1024];

byte static_buf128[128];


void ext_rom_api_write(byte Value);
void add_to_out_buf(byte value);
void add_block_to_out_buf(int size,byte *block);
byte ext_rom_api_read(void);



void init_extrom(void) {
  if (ext_rom_mode) {
    if (strlen(ext_rom_emu_folder)>0) {
      printf("ExtROM_EMU_SD folder   : %s\n",ext_rom_emu_folder);
    }
    printf("ExtROM                 : %s\n",ext_rom_file_name);

    sprintf(tmp_path,"%s%s",ext_rom_emu_folder,ext_rom_file_name);
    extrom_file=fopen(tmp_path,"rb");
    if (extrom_file == 0) {
      printf("WARNING: Ошибка открытия файла образа ROM - %s\n",tmp_path);
      printf("ExtRom Boot - режим отключен\n");

      ext_rom_mode=0;
    }
    emu_stage=EMU_STAGE1;
  }
}

byte ext_rom_read(unsigned char PPI3_B, unsigned char PPI3_C) {
	unsigned char value=0;
	unsigned int ext_rom_addr=(PPI3_C<<8)+PPI3_B;
	if (ext_rom_mode) {
    if (ext_rom_addr_changed) {
		  fseek(extrom_file,ext_rom_addr,SEEK_SET);
		  value=fgetc(extrom_file);
		  // printf("EXTROM: %04x=%02x\n",ext_rom_addr,value);
    }
	}
	return value;
}

void emu_read_image_128(void) {
  //CMD,DRV,TRK,SEC
  int offset=(e_trk*40+e_sec)*128;

  // fseek(f_emu,(trk*40+sec)*128,SEEK_SET);

  // printf("READ CMD: %02x, DRV:%02x, TRK: %03d, SEC: %02d OFFSET:%08x\n",e_cmd,e_drv,e_trk,e_sec,offset);
  sprintf(tmp_path,"%sdisk_%c.kdi",ext_rom_emu_folder,'a'+e_drv);
  f_emu=fopen(tmp_path,"rb");
  if (f_emu == 0) {
    printf("ERROR: can't open %s\n",tmp_path);
  } else {
    fseek(f_emu,offset,SEEK_SET);
    fread(static_buf128,1,128,f_emu);
    fclose(f_emu);
  }
}

void emu_write128(void) {
  //CMD,DRV,TRK,SEC
  int offset=(e_trk*40+e_sec)*128;

  // fseek(f_emu,(trk*40+sec)*128,SEEK_SET);
  // printf("WRITE CMD: %02x, DRV:%02x, TRK: %03d, SEC: %02d OFFSET:%08x\n",e_cmd,e_drv,e_trk,e_sec,offset);

  sprintf(tmp_path,"%sdisk_%c.kdi",ext_rom_emu_folder,'a'+e_drv);
  f_emu=fopen(tmp_path,"r+b");
  if (f_emu == 0) {
    printf("ERROR: can't open for write %s\n",tmp_path);
  } else {
    fseek(f_emu,offset,SEEK_SET);
    fwrite(in_buffer,1,128,f_emu);
    fclose(f_emu);
  }
}

void emu_stage1(void) {

  sprintf(tmp_path,"%s%s",ext_rom_emu_folder,"stage2.rom");
  if ( (in_buffer[0]>=1) && (in_buffer[0]<=7) ) {
    sprintf(tmp_path,"%srom%c.rom",ext_rom_emu_folder,'0'+in_buffer[0]);
  }

  printf("requested file: %s\n",tmp_path);

  f_emu=fopen(tmp_path,"rb");
  if (f_emu == 0) {
    printf("ERROR: Stage1 loader request '%d' file, but file '%s' can't be opened\n",in_buffer[0],tmp_path);
  } else {
    in_buffer_size=0;
    out_buffer_size=fread(out_buffer+2,1,EXT_BUF_SIZE,f_emu);
    fclose(f_emu);

    out_buffer[0]=out_buffer[2+6];    // отсылаем загрузчику адрес размещения файла в памяти
    out_buffer[1]=out_buffer_size>>8; // отсылаем загрузчику адрес размещения файла в памяти
    out_buffer_size+=2;               // два первых байта
    emu_stage=EMU_STAGE2_WAITCMD;
  }  
}


void emu_api_cmd(void) {
  int i;
  switch (in_buffer[0]) {
  
    case 0: { // ping
      add_to_out_buf(EMU_API_OK);
      in_buffer_size=0;
      break;
    }

    case 1: { // read sector
      emu_read_image_128();
      add_block_to_out_buf(128,static_buf128);
      in_buffer_size=0;
      break;
    }

    case 2: { // write sector
      emu_stage=EMU_STAGE2_WRITE128;
      in_buffer_size=0;
      break;
    }
  
    case 0xA0: { // set system substitute trk=1 on,=0 off
      e_substitute_system_track=e_trk;
      in_buffer_size=0;
      break;
    }


    case 0xf0: {// SPEED TEST - out 0x8000 bytes
      printf("SDEMU: SPEDD TEST 8000 to korvet\n");
      for (i=0;i<0x8000;i++){ add_to_out_buf(0);}
      // out_buffer_size=0x8000;
      in_buffer_size=0;
      break;
    }

    case 0xf1: {// SPEED TEST - in 0x8000 bytes
      printf("SDEMU: SPEDD TEST 8000 from korvet\n");
      emu_stage=EMU_STAGE2_WRSPPEDTEST;
      in_buffer_size=0;
      break;
    }

    default: { 
      printf("SDEMU: unsupported CMD '%02x'\n",in_buffer[0]);
      break;
    }
  }
}

void parse_write(void) {
  int crc;
  int result_status=0;

  // char fname[1024]="extrom/stage2.rom";
  switch (emu_stage) {

    case EMU_STAGE1: { // Stage1 loader send GET_IMAGE_X where X-0..8 (8 by default) 
      emu_stage1();
      break;
    }

    case EMU_STAGE2_WAITCMD: {
      if (in_buffer_size == 5) {
        e_cmd=in_buffer[0];
        e_drv=in_buffer[1];
        e_trk=in_buffer[2];
        e_sec=in_buffer[3];
        e_crc=in_buffer[4];
        crc=e_cmd+e_drv+e_trk+e_sec-1;

        printf("CMD: %02x, DRV:%02x, TRK: %03d, SEC: %02d CRC:%02x (%02x)%s\n",e_cmd,e_drv,e_trk,e_sec,e_crc,crc, e_crc==crc ? "" : " - ERROR !!!");

        if (crc == e_crc) {
          add_to_out_buf(EMU_API_OK);
          emu_api_cmd();
        } else {
          add_to_out_buf(EMU_API_FAIL);
        }
      }
      break;
    }
    case EMU_STAGE2_WRITE128: {
      if (in_buffer_size == 128) {
        emu_write128();
        in_buffer_size=0;
        emu_stage=EMU_STAGE2_WAITCMD;
      }
      break;
    }
    case EMU_STAGE2_WRSPPEDTEST: {
      if (in_buffer_size == 0x8000) {
        in_buffer_size=0;
        emu_stage=EMU_STAGE2_WAITCMD;
      }
      break;
    }
    default: {
        printf("ERROR: unsupported stage '%d' size:%d\n",emu_stage,in_buffer_size);
    }
  }
  return;  
}

void ext_rom_api_write(byte Value) {
  if (in_buffer_size == EXT_BUF_SIZE) {
    printf("WARNING: in_buffer FULL, ignore\n");
  } else {
    // printf("DBG: in_buffer: %02d=%02x\n",in_buffer_size,Value);
    in_buffer[in_buffer_size++]=Value;
  }
  parse_write();
}

void add_to_out_buf(byte Value) {
  if (out_buffer_size == EXT_BUF_SIZE) {
    printf("WARNING: out_buffer FULL, ignore\n");
  } else {
    // printf("DBG: in_buffer: %02d=%02x\n",out_buffer_size,Value);
    out_buffer[out_buffer_size++]=Value;
  }
}

void add_block_to_out_buf(int size,byte *block) {
  int i;
  for (i=0;i<size;i++) {
    add_to_out_buf(block[i]);
  }
}

byte ext_rom_api_read(void) {
  byte Value;
  if (out_buffer_size == 0) {
    printf("WARNING: out_buffer_size Read from empty buffer, ignore\n");
  } else {
    Value=out_buffer[0];
    out_buffer_size--;
    if (out_buffer_size>0) {
      memmove(out_buffer,out_buffer+1,out_buffer_size);
    }
  }
  return Value;
}

