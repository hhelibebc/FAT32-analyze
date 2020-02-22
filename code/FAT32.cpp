#include "FAT32.h"

#define Cluster2Sector(n) (drw.RootStartSector+(n-2)*sect0.SectorsPerCluster)

static LPCSTR szDiscFile = "\\\\.\\F:";
static DWORD dwRealNumber;
static PDWORD pFatRecord;
static bool bRet;
static Tree tt;
static HANDLE hDisk;
static CATALOG *pCatalog;
static char* record_type[] = {"FILE","DIR"};
static char* size_unit = "BKMG";

BPB sect0;
DiskRW drw;
char buf[4096],strbuf[256],*pStr = strbuf+100;

Tree cata;

static void _write(const char* name){
	FILE* fi;
	fi = fopen(name,"w");
	fwrite(buf,1,512,fi);
	fclose(fi);
}
static void uni2mbs(char* dest,LPCWSTR src){
	wcstombs(dest, src, MAX_PATH);
}
static void GetOthers(Tree* pt,CATALOG* pc){
	pt->sub.empty();
	pt->name.clear();
	pt->offset =  pc->S.StartClusterH<<16|pc->S.StartClusterL;
	pt->len = pc->S.FileLengthInByte;
	pt->isdir = 1 - pc->S.Property.bits.FI;
}
static void GetLongName(Tree* pt,CATALOG* pc){
	int cnt = 0;
	char *p = strbuf;
	GetOthers(pt,pc);
	do{
		pc--;
		memcpy(p+cnt,pc->L.FileName0_4,10);
		memcpy(p+cnt+10,pc->L.FileName5_10,12);
		memcpy(p+cnt+22,pc->L.FileName11_12,4);
		cnt += 26;
	}while(!pc->L.Property.bits.EndFlag);
	p[cnt++] = 0;// 添加Unicode字符串结束符
	p[cnt++] = 0;
	uni2mbs(p+cnt,(LPCWSTR)p);
	pt->name = p + cnt;
}
static void GetShortName(Tree* pt,CATALOG* pc){
	char* tmp;
	int len;
	GetOthers(pt,pc);
	memcpy(strbuf,pc->S.FileName,8);
	tmp = strbuf+7;
	while(*tmp-- == ' ') ;
	len = tmp - strbuf + 2;
	tmp = strbuf + len;
	if(pt->isdir == 0)
		*tmp++ = '.';
	memcpy(tmp,pc->S.Ext,3);
	len = 0;
	while(*tmp++ != ' ' && len++ < 3) ;
	tmp[-1] = 0;
	pt->name = strbuf;
}
static void _print_str(const char* s,int len){
	int old = strlen(s);
	strncpy(pStr,s,len);
	if(old < len)
		memset(pStr+old,' ',len-old+1);
	pStr[len] = 0;
	printf("%s",pStr);
}

DiskRW::DiskRW(){
	_wsetlocale(LC_ALL,L"");// 仅需初始化一次
}
void DiskRW::Open(){
	hDisk = CreateFile(szDiscFile, GENERIC_READ|GENERIC_WRITE,  FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}
void DiskRW::Close(){
	bRet = CloseHandle(hDisk);
}
void DiskRW::CreateFileSystem(){
	ReadSector(buf,0,1);
	memcpy(&sect0,buf,sizeof(sect0));
	Fat1StartSector = sect0.rsvSectors;
	RootStartSector = Fat1StartSector + sect0.cntFAT * sect0.SectorsPerFAT_L 
		+ (sect0.RootStartCluster - 2) * sect0.SectorsPerCluster;
	cata.name = "root";
	cata.offset = sect0.RootStartCluster;
	cata.len = 0;
	cata.isdir = 1;
	//ReadDir(&cata);
}

void ReadSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	bRet = ReadFile(hDisk, p, len<<9, &dwRealNumber, NULL);
}
void WriteSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	bRet = WriteFile(hDisk, p, len<<9, &dwRealNumber, NULL);
}

void ReadDir(Tree* pt){
	if(1 == pt->isdir){
		ReadSector(buf,Cluster2Sector(pt->offset),8);
		pCatalog = (CATALOG *)buf;
		if(pCatalog->S.Property.bits.SUB == 1)
			pCatalog += 2;
		else
			pCatalog += 1;
		while(pCatalog->S.FileName[0]){
		if(pCatalog->L.Property.all != 0xe5){
			if(pCatalog->L.LongEntryFlag != 0x0f){
				GetShortName(&tt,pCatalog);
				pCatalog++;
			}
			else{
				int i = pCatalog->L.Property.bits.Ind;
				GetLongName(&tt,pCatalog+i);
				pCatalog += i+1;
			}
			pt->sub.push_back(tt);
		}
		else// 如果目录项已删除，不应执行后续
			pCatalog++;
	}
	}
	Travel(pt,ReadDir,0);
}
void Print(Tree* pt){
	sprintf(strbuf,"name:%s",pt->name.data());
	_print_str(strbuf,50);
	/* 另一种显示方案
	float i = pt->len;
	int j = 0;
	while(i >= 1000){ 
		i = i / 1024;
		j++;
	}
	if(i >= 100)
		sprintf(strbuf,"size:%4d %c",(int)i,size_unit[j]);
	else if(i >= 10)
		sprintf(strbuf,"size:%4.1f %c",i,size_unit[j]);
	else if(i > 0)
		sprintf(strbuf,"size:%4.2f %c",i,size_unit[j]);
	else
		sprintf(strbuf,"size:   0");*/
	sprintf(strbuf,"size:%d",pt->len);
	_print_str(strbuf,16);
	sprintf(strbuf,"offset:0x%X",pt->offset);
	_print_str(strbuf,16);
	sprintf(strbuf,"type:%s",record_type[pt->isdir]);
	_print_str(strbuf,10);
	printf("\n");
}
void Write(Tree* pt){
	if(pt->isdir == 1){
		ReadSector(buf,pt->offset,1);
		_write(pt->name.data());
	}
}
void Travel(Tree* pt,TravelFun pf,int mode){
	if(1 == mode)
		pf(pt);
	int subs = pt->sub.size();
	for(int i=0;i<subs;i++)
		Travel(&pt->sub[i],pf,mode);
}
