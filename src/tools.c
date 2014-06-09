#include "korvet.h"
#include "vg.h"
#include <assert.h>

#ifdef DBG
#include "dbg/dbg.h"
#endif

extern byte LUT[16];

extern int SCREEN_OFFX;
extern int SCREEN_OFFY;
extern int SCREEN_XMAX;
extern int SCREEN_YMAX;
extern int SCREEN_OSDY;

extern int OSD_LUT_Flag;
extern int OSD_FPS_Flag;
extern int OSD_FDD_Flag;

extern int FPS_Scr;
extern int FPS_LED;

extern char FontFileName[1024];
extern char RomFileName[1024];
extern char MapperFileName[1024];

extern int BW_Flag;

extern int scr_Wide_Mode;
extern int scr_Second_Font;
extern int scr_Page_Show;

extern int FlagScreenScale;
extern int Current_Scr_Mode;

extern byte RAM[65535];
extern byte ACZU[1024*2];          // 1К памяти АЦЗУ
extern byte GZU[4][PLANESIZE*3]; // 3 слоя ГЗУ (4 страницы)
extern PALLETE pallete;

extern int AllScreenUpdateFlag;
extern int LutUpdateFlag;

extern int scr_GZU_Size_Mask;    // маска размера ГЗУ, =0x0f - 4*48, =0 - 1x 48k

extern int KeyboadUpdateFlag;   // =1 if need rebuld keyboard layout
extern int KeyboardLayout;

extern int  InUseFDD[4];

extern byte SOUNDBUF[];

const char AboutMSG[]="Korvet Emulator by Sergey Erokhin & Korvet Team|pk8020@narod.ru|2012-05-30|V1.?.1";

extern AUDIOSTREAM *stream;

int BMP_NUM=0;

void Debug_LUT(int Debug_Key) {
    byte SaveLut[16];
    byte i;
    for (i=0; i<16; i++) {
        SaveLut[i]=LUT[i];
        LUT[i]=i;
    }
    LUT_Update(BW_Flag);
    while (key[Debug_Key]);
    for (i=0; i<16; i++) {
        LUT[i]=SaveLut[i];
    }
    LUT_Update(BW_Flag);
}

void Write_BMP(char * FileName,int page) {
    int kx=1;
    int saved_page=scr_Page_Show;
    if (FlagScreenScale) kx=2;
    BITMAP *bmp=create_bitmap(512*kx,256*kx);
    clear_bitmap(bmp);

    scr_Page_Show=page;
    AllScreenUpdateFlag=1;
    SCREEN_ShowScreen();

    blit(screen,bmp,SCREEN_OFFX,SCREEN_OFFY,0,0,512*kx,256*kx);
    save_bmp(FileName,bmp,pallete);

    scr_Page_Show=saved_page;
    AllScreenUpdateFlag=1;
    SCREEN_ShowScreen();
}


void dump_gzu(int page) {
    char fname[512];
    int i;
    byte B1[3][PLANESIZE];
    FILE *F_DMP;

    sprintf(fname,"DMP.LGZU%d",page);

    F_DMP=fopen(fname,"wb");
    for (i=0; i<PLANESIZE; i++) {
        B1[0][i]=GZU[page][i*4+0];
        B1[1][i]=GZU[page][i*4+1];
        B1[2][i]=GZU[page][i*4+2];
        //+3 - aczu
    }
    fwrite(B1,PLANESIZE*3,1,F_DMP);
    fclose(F_DMP);
}

void Write_Dump(void)
{

    FILE *F_DMP;

    int i,j;
    word reg;
    char BUF[1024];

    F_DMP=fopen("DMP.RAM","wb");
    fwrite(RAM,0x10000,1,F_DMP);
    fclose(F_DMP);

    F_DMP=fopen("DMP.ACZU","wb");
    fwrite(ACZU,1024*2,1,F_DMP);
    fclose(F_DMP);

    dump_gzu(0);
    dump_gzu(1);
    dump_gzu(2);
    dump_gzu(3);

    F_DMP=fopen("DMP.LUT","wb");
    fwrite(LUT,sizeof(LUT),1,F_DMP);
    fclose(F_DMP);

    F_DMP=fopen("DMP.CPU","wb");
    reg=CPU_GetPC();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    reg=CPU_GetSP();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    reg=CPU_GetAF();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    reg=CPU_GetHL();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    reg=CPU_GetDE();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    reg=CPU_GetBC();
    fwrite(&reg,sizeof(reg),1,F_DMP);
    fclose(F_DMP);

    sprintf(BUF,"DMPSC%03d_0.bmp",BMP_NUM);
    Write_BMP(BUF,0);
    BMP_NUM++;

}

void ReadConfig(void) {
    char section[]="korvet";
    set_config_file("./korvet.cfg");
    strcpy(Disks[0]      ,get_config_string(section,"DriveA","disk/disk.kdi"));
    strcpy(Disks[1]      ,get_config_string(section,"DriveB","disk/disk1.kdi"));
    strcpy(Disks[2]      ,get_config_string(section,"DriveC","disk/disk2.kdi"));
    strcpy(Disks[3]      ,get_config_string(section,"DriveD","disk/disk3.kdi"));

    strcpy(FontFileName  ,get_config_string(section,"FONT","data/korvet2.fnt"));
    strcpy(RomFileName   ,get_config_string(section,"ROM","data/rom.rom"));
    strcpy(MapperFileName,get_config_string(section,"MAPPER","data/mapper.mem"));

    scr_GZU_Size_Mask    =get_config_hex(section,"GZU_Pages",4);
    scr_GZU_Size_Mask    =(scr_GZU_Size_Mask == 1) ? 0:0x0f;

    OSD_LUT_Flag         =get_config_hex(section,"OSD_LUT",0);
    OSD_FPS_Flag         =get_config_hex(section,"OSD_FPS",0);
    OSD_FDD_Flag         =get_config_hex(section,"OSD_FDD",0);

    FlagScreenScale      =get_config_hex(section,"SCALE_WINDOW",0);

    KeyboardLayout       =get_config_hex(section,"KEYBOARD_MODE",KBD_AUTO);
}

void PrintDecor() {

    rect(screen,SCREEN_OFFX-2,SCREEN_OFFY-2,SCREEN_OFFX+SCREEN_XMAX+1,SCREEN_OFFY+SCREEN_YMAX+1,255);

    //init InUseFlag for screen update
    InUseFDD[0]=InUseFDD[1]=InUseFDD[2]=InUseFDD[3]=1;

    if (Current_Scr_Mode != SCR_DBG) {
        textprintf_ex(screen,font,15,SCREEN_H-33,255,0,"Alt-F9-MENU, F11-Reset, F12-Quit, F8-Zoom, F6-Turbo, F10-Mono, F9-dbg");
        rect(screen,0,SCREEN_H-40,SCREEN_W,SCREEN_H-40,255);
        textprintf_ex(screen,font,0,SCREEN_H-16,255,0,AboutMSG);
    }
}

void MUTE_BUF(void) {
    int i;
    unsigned char *p;

    for(i=0; i<AUDIO_BUFFER_SIZE; i++) {
        SOUNDBUF[i]=0;
    }

    while (!(p = get_audio_stream_buffer(stream))) rest(0);
    memcpy(p,SOUNDBUF,AUDIO_BUFFER_SIZE);
    free_audio_stream_buffer(stream);
}


void parse_command_line(int argc,char **argv) {
    int i;
    // parse command line option -A filename -B filename
    while ((i=getopt(argc, argv, "a:A:b:B:c:C:d:D:")) != -1) {
        switch (tolower(i)) {
        case 'a':
            strcpy(Disks[0],optarg);
            break;
        case 'b':
            strcpy(Disks[1],optarg);
            break;
        case 'c':
            strcpy(Disks[2],optarg);
            break;
        case 'd':
            strcpy(Disks[3],optarg);
            break;
        }
    }
}

void check_missing_images(void) {
    int     i;
    int     errors_count=0;
    int     file;

    for (i=0; i<4; i++) {
        file=open(Disks[i],O_RDONLY);
        if (file<0) {
            printf("Warning: Can't open drive '%c' file: %s\n",'A'+i,Disks[i]);
            errors_count++;
        }
        close(file);
    }

    if (errors_count) {
        printf("Press Enter to continue\n");
        getchar();
    }
}

void update_osd(void) {
    // выводим OnScreen LED
    // ТОЛЬКО если есть необходимость обновить индикаторы,
    // иначе будут мигать, да и FPS падает ;-)
    // FPS
    if (OSD_FPS_Flag && (FPS_Scr != FPS_LED)) {
        PutLED_FPS(SCREEN_OFFX,SCREEN_OSDY  ,FPS_Scr);
        FPS_LED=FPS_Scr;
    };
    // Floppy Disk TRACK
    if (OSD_FDD_Flag && InUseFDD[0]) {
        InUseFDD[0]--;
        PutLED_FDD(SCREEN_OFFX+512-80,SCREEN_OSDY,VG.TrackReal[0],InUseFDD[0]);
    }
    if (OSD_FDD_Flag && InUseFDD[1]) {
        InUseFDD[1]--;
        PutLED_FDD(SCREEN_OFFX+512-60,SCREEN_OSDY,VG.TrackReal[1],InUseFDD[1]);
    }
    if (OSD_FDD_Flag && InUseFDD[2]) {
        InUseFDD[2]--;
        PutLED_FDD(SCREEN_OFFX+512-40,SCREEN_OSDY,VG.TrackReal[2],InUseFDD[2]);
    }
    if (OSD_FDD_Flag && InUseFDD[3]) {
        InUseFDD[3]--;
        PutLED_FDD(SCREEN_OFFX+512-20,SCREEN_OSDY,VG.TrackReal[3],InUseFDD[3]);
    }
    // if (JoystickUseFlag) {JoystickUseFlag--;textprintf(screen,font,SCREEN_OFFX+512,SCREEN_OSDY,255,"%s",(JoystickUseFlag==0)?"      ":"JOY:3B");}

    if (getpixel(screen,SCREEN_OFFX-2,SCREEN_OFFY-2) != 255) {
        PrintDecor();
        AllScreenUpdateFlag=1;
    }

}