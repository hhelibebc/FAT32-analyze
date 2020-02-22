#include "stdafx.h"
#include "FAT32.h"

static LPCSTR szDiscFile = "\\\\.\\F:";
static DWORD dwRealNumber;
static bool bRet;
static PDWORD pFatRecord;
static Tree tt;

BPB sect0;
DiskRW drw;
char buf[4096];
char strbuf[256];
Tree cata;

DiskRW::DiskRW(){
	_wsetlocale(LC_ALL,L"");// 仅需初始化一次
}
void DiskRW::Open(){
	hDisk = CreateFile(szDiscFile, GENERIC_READ|GENERIC_WRITE,  FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}
void DiskRW::Close(){
	bRet = CloseHandle(hDisk);
}
void DiskRW::ReadSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	bRet = ReadFile(hDisk, p, len<<9, &dwRealNumber, NULL);
	int err = GetLastError();
}
void DiskRW::WriteSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	bRet = WriteFile(hDisk, p, len<<9, &dwRealNumber, NULL);
}
void DiskRW::CreateFileSystem(){
	ReadSector(buf,0,1);
	memcpy(&sect0,buf,sizeof(sect0));
	Fat1StartSector = sect0.rsvSectors;
	Fat2StartSector = Fat1StartSector + sect0.SectorsPerFAT_L;
	RootStartSector = Fat2StartSector + sect0.SectorsPerFAT_L + (sect0.RootStartCluster - 2) * sect0.SectorsPerCluster;
	cata.name = "root";
	cata.offset = drw.RootStartSector;
	cata.len = 0;
	cata.isdir = 1;
}
int DiskRW::ReadDir(Tree* pt,int si){
	int RecordNum = 0,i,j;
	ReadSector(buf,si,8);
	pCatalog = (CATALOG *)buf;
	if(pCatalog->S.Property.bits.SUB == 1)
		pCatalog += 2;
	else
		pCatalog += 1;
	while(pCatalog->S.FileName[0]){
		j = RecordNum>>4;
		if(pCatalog->L.Property.all != 0xe5){
			if(pCatalog->L.LongEntryFlag != 0x0f){
				GetShortName(&tt,pCatalog);
				pCatalog++;
			}
			else{
				i = pCatalog->L.Property.bits.Ind;
				GetLongName(&tt,pCatalog+i);
				pCatalog += i+1;
			}
			pt->sub.push_back(tt);
		}
		else{// 如果目录项已删除，不应执行后续
			pCatalog++;
		}
	}
	Tree* p;
	int subs = pt->sub.size();
	for(i=0;i<subs;i++){
		p = &pt->sub[i];
		if(p->isdir)
			ReadDir(p,p->offset);
	}
	pCatalog = (CATALOG *)buf;
	return RecordNum;
}
int DiskRW::Cluster2Sector(int n){
	return (RootStartSector+((n-2)<<5));
}
void DiskRW::GetLongName(Tree* pt,CATALOG* pc){
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
void DiskRW::GetShortName(Tree* pt,CATALOG* pc){
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
void DiskRW::GetOthers(Tree* pt,CATALOG* pc){
	pt->sub.empty();
	pt->name.clear();
	pt->offset = Cluster2Sector(pc->S.StartClusterH<<16|pc->S.StartClusterL);
	pt->len = pc->S.FileLengthInByte;
	pt->isdir = 1 - pc->S.Property.bits.FI;
}
void Print(Tree* pt){
	printf("name:%30s size:%8d offset:0x%8X isdir:%d\n",pt->name.data(),pt->len,pt->offset,pt->isdir);
}
void Write(Tree* pt){
	if(pt->isdir == 1){
		drw.ReadSector(buf,pt->offset,1);
		_write(pt->name.data());
	}
}
void _write(const char* name){
	FILE* fi;
	fi = fopen(name,"w");
	fwrite(buf,1,512,fi);
	fclose(fi);
}
void uni2mbs(char* dest,LPCWSTR src){
	wcstombs(dest, src, MAX_PATH);
}
void DiskRW::Travel(Tree* pt,TravelFun pf){
	int subs = pt->sub.size();
	pf(pt);
	for(int i=0;i<subs;i++)
		Travel(&pt->sub[i],pf);
}
