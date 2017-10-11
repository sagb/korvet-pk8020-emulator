﻿/**************************************************************************
Эмуляция контроллера дисководов
**************************************************************************/
#include "korvet.h"

/*
00h (DC_READSTATUS)   - чтение состояния ВГ (вызов при чтении из порта 18h).
01h (DC_WRITECOMMAND) - запись команды ВГ (вызов при записи в порт 18h).
02h (DC_READTRACK)    - чтение дорожки (вызов при чтении из порта 19h).
03h (DC_WRITETRACK)   - запись дорожки (вызов при записи в порт 19h).
04h (DC_READSECTOR)   - чтение сектора (вызов при чтении из порта 1Ah).
05h (DC_WRITESECTOR)  - запись сектора (вызов при записи в порт 1Ah).
06h (DC_READDATA)     - чтение данных (вызов при чтении из порта 1Bh).
07h (DC_WRITEDATA)    - запись данных (вызов при записи в порт 1Bh).
08h (DC_READDRQINTRQ) - чтение сигналов Intrq и Drq
                        (вызов при чтении из порта 39h).
09h (DC_WRITESYSTEM)  - запись системного регистра контроллера дисководов
                        (вызов при записи в порт 39h).
0Ah (DC_INIT)         - инициализация диска (вызов при включении эмуляции и
                        смене диска). При вызове этой функции текущая
                        директория - директория с файлами образа диска.
0Bh (DC_UNINIT)       - снятие диска (вызов при выключении эмуляции и смене диска).
0Ch (DC_RESET)        - Reset контроллера дисковода.
*/
/* Данные контроллера дисковода */
struct DatVG {
	byte        DataIO;       // Значение, которое передается через порт
	byte        OperIO;       // Операция ВГ
	byte        RegStatus;    // Регистр состояния ВГ
	byte        RegCom;       // Регистр команд
	signed char RegTrack;     // Регистр дорожки
	byte        RegSect;      // Регистр сектора
	byte        RegData;      // Регистр данных
	byte        System;       // Системный регистр контроллера дисковода
	signed char StepDirect;   // Направление шага: -01 - назад; 01 - вперед
	byte        TrackReal[4]; // Положение головок дисководов
	byte        reserved[3];
};


void DskVG(void);
void DiskVG(byte Oper);

extern struct DatVG VG;
extern struct DatVG VG;
extern char   Disks[4][1024];
extern char   DskPth[];
extern char   StPth[];

