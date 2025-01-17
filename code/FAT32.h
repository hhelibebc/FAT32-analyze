﻿#pragma once

#include <Windows.h>
#include <string>
#include <vector>
using namespace std;
#pragma pack(1)

typedef enum{
	_DEFAULT = 0,
	_READ = 1,
	_WRITE = 2,
	_REMOVE = 4,
	FIND_EXIST_RECORD,
	FIND_FREE_RECORD,
	READ_RECORD,
}CONST_VALUE;
typedef struct{
	char rsv1[11];
	WORD BytesPerSector;    // 每扇区字节数
	BYTE SectorsPerCluster; // 每簇扇区数
	WORD rsvSectors;        // 保留扇区数
	BYTE cntFAT;            // FAT表个数
	char rsv2[2];
	WORD TotalSectors_S;    // 每FAT表扇区数
	BYTE StorageMedium;     // 存储介质描述符
	WORD SectorsPerFAT_S;   // 每FAT表扇区数
	WORD SectorsPerTrack;   // 每磁道扇区数
	WORD Heads;             // 磁头数
	DWORD HideSectors;      // 隐藏扇区数
	DWORD TotalSectors_L;   // 总扇区数
	DWORD SectorsPerFAT_L;  // 每FAT表扇区数
	char rsv3[4];
	DWORD RootStartCluster; // 根目录起始簇
	WORD SectorsPerBoot;    // Boot扇区数
	WORD BakBootLocation;   // 备份引导扇区位置
	char rsv4[19];
	char VolumeLabel[10];   // 卷标
	char FileSystem[8];     // 文件系统
}BPB;
typedef struct{
	WORD xHalfSec:5;
	WORD xMinute:6;
	WORD xHour:5;
}TIME_t;
typedef struct{
	WORD xDay:5;
	WORD xMonth:4;
	WORD xYearFrom1980:7;
}DATE_t;
typedef union{
	struct{
		char FileName[8];
		char Ext[3];
		union{
			struct{
				BYTE RO:1;
				BYTE HIDE:1;
				BYTE SYS:1;
				BYTE VL:1;
				BYTE SUB:1;
				BYTE FI:1;
				BYTE UNDEFINED:2;
			}bits;
			BYTE all;
		}Property;
		char rsv1[1];
		BYTE x10ms;
		TIME_t CreateTime;
		DATE_t CreateDate;
		DATE_t LastAccessDate;
		WORD StartClusterH;
		TIME_t LastChangeTime;
		DATE_t LastChangeDate;
		WORD StartClusterL;
		DWORD FileLengthInByte;
	}S;
	struct{
		union{
			struct{
				BYTE Ind:5;
				BYTE rsv1:1;
				BYTE EndFlag:1;
				BYTE rsv2:1;
			}bits;
			BYTE all;
		}Property;
		WORD FileName0_4[5];
		BYTE LongEntryFlag;
		BYTE rsv3;
		BYTE CheckCode;
		WORD FileName5_10[6];
		WORD StartCluster;
		WORD FileName11_12[2];
	}L;
}CATALOG;
typedef struct Tree{
	vector<Tree> sub;
	string name;
	int offset;
	int len;
	int isdir;
}Tree;
typedef void (*TravelFun)(Tree* pt);
class DiskRW{
public:
	void Open();
	void Close();
	void Format();
	void CreateFileSystem();
	void WriteCluster(int dest);
	void ReadCluster(int dest);
public:
	DWORD Fat1StartSector;
	DWORD Fat2StartSector;
	DWORD RootStartSector;
	Tree cata;
	char buf[16384];
	int CurRecordCluster;
};
class FileRW{
public:
	void open(const char* name,int right);
	void close();
	void create(const char* name);
	void remove(const char* name);
	void read(void* p,int len);
	void write(void* p,int len);
	int GetRecord(int mode,int arg);
	void SetRecord(int offset,int len,int isdir);
	Tree* find(Tree* pt,const char* name);
	Tree* _find(Tree* pt,const char* name);
	DWORD next_fat();
	DWORD next_free_fat(int si);
	DWORD getvar(int ind);
	void setvar(int ind,DWORD var);
	void Update(int mode);
public:
	DWORD fat[128];
	Tree* pCurrent;
	Tree* pParent;
	int CurFatInd;
	int CurFatPage;
	int RecordCluster;
	int RecordOffset;
	int FreeCluster;
	int Access;
};

extern DiskRW drw;
extern FileRW frw;

extern void ReadSector(void* p,int si,int len);
extern void WriteSector(void* p,int si,int len);
extern void Travel(Tree *pt,TravelFun pf,int mode);
extern void ReadDir(Tree* pt);
extern void Write(Tree* pt);
extern void Print(Tree* pt);