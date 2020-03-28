#include "FAT32.h"

#define Cluster2Sector(n) (drw.RootStartSector+((n-2)<<SPC_BITS))

static LPCSTR szDiscFile = "\\\\.\\F:";
static DWORD dwRealNumber;
static Tree tt;
static HANDLE hDisk;
static CATALOG *pc1,*pc2;
static char* record_type[] = {"FILE","DIR"};
static char* size_unit = "BKMG";
const static int SPC = 32,SPC_MASK = SPC-1,SPC_BITS = 5;
const static int FAT = 128,FAT_MASK = FAT-1,FAT_BITS = 7;

BPB sect0;
DiskRW drw;
FileRW frw;
char strbuf[512],*pStr = strbuf+256;

static int CalCheckCode(const char* name){
	BYTE sum = 0;
	for(int i=0;i<11;i++)
		sum = (sum & 1 ? 0x80 : 0) + (sum >> 1) + name[i];
	return sum;
}
static void split_str(char* s1,char* s2,const char* name,char ch){
	int len = strlen(name);
	char* p = s1 + len;
	strcpy(s1,name);
	while(ch != *p--) ;
	p[1] = 0;
	strcpy(s2,p+2);
}
static void _write(const char* name){
	FILE* fi;
	fi = fopen(name,"w");
	fwrite(drw.buf,1,512,fi);
	fclose(fi);
}
static void uni2mbs(char* dest,LPCWSTR src){
	wcstombs(dest, src, MAX_PATH);
}
static void GetOthers(){
	tt.sub.empty();
	tt.name.clear();
	tt.offset =  pc2->S.StartClusterH<<16|pc2->S.StartClusterL;
	tt.len = pc2->S.FileLengthInByte;
	tt.isdir = 1 - pc2->S.Property.bits.FI;
}
static void GetLongName(){
	LPWSTR p = (LPWSTR)strbuf;
	
	pc1 += pc1->L.Property.bits.Ind + 1;
	pc2 = pc1 - 1;
	GetOthers();
	do{
		pc2--;
		memcpy(p,pc2->L.FileName0_4,10);
		p += 5;
		memcpy(p,pc2->L.FileName5_10,12);
		p += 6;
		memcpy(p,pc2->L.FileName11_12,4);
		p += 2;
	}while(!pc2->L.Property.bits.EndFlag);
	*p++ = 0;// 添加Unicode字符串结束符
	uni2mbs((char*)p,(LPCWSTR)strbuf);
	tt.name = (char*)p;
}
static void GetShortName(){
	char* tmp;
	int len;

	pc2 = pc1;
	pc1++;
	GetOthers();
	memcpy(strbuf,pc2->S.FileName,8);
	tmp = strbuf+7;
	while(*tmp-- == ' ') ;
	len = tmp - strbuf + 2;
	tmp = strbuf + len;
	if(tt.isdir == 0)
		*tmp++ = '.';
	memcpy(tmp,pc2->S.Ext,3);
	len = 0;
	while(*tmp++ != ' ' && len++ < 3) ;
	tmp[-1] = 0;
	tt.name = strbuf;
}
static void SetOthers(int offset,int len,int isdir){
	tt.offset = offset;
	tt.len = len;
	tt.isdir = isdir;
	frw.pCurrent->sub.push_back(tt);
}
static void SetShortName(const char* name,int type){
	/*if(0 != pc1->S.FileName[0] && 0xe5 != pc1->S.FileName[0])
		drw.ReadCluster(tt.offset);*/
	memset(pc1->S.FileName,0x20,11);
	if(0 == type){
		split_str(pStr,pStr+128,name,'.');
		memcpy(pc1->S.FileName,pStr,min(strlen(pStr),8));
		memcpy(pc1->S.Ext,pStr+128,min(strlen(pStr+128),3));
	}
	else
		memcpy(pc1->S.FileName,name,8);
	memcpy(pc1->S.FileName+6,"~1",2);
}
static void SetLongName(const char* name,int type){
	LPWSTR p = (LPWSTR)strbuf;
	int total,needs;
	strcpy(pStr,name);
	mbstowcs(p,pStr,MAX_PATH);
	total = wcslen(p);
	needs = (total+12)/13;
	memset(p+total+1,0xff,(needs*13-total-1)*2);
	frw.RecordOffset = frw.GetRecord(FIND_FREE_RECORD,needs+1);
	pc1 = (CATALOG*)drw.buf + frw.RecordOffset + needs;
	memset(pc1,0,sizeof(CATALOG) * (needs+1));
	pc2 = pc1-1;

	SetShortName(pStr,type);
	total = CalCheckCode(pc1->S.FileName);

	for(int i=0;i<needs;i++){
		pc2->L.Property.bits.Ind = i + 1;
		pc2->L.LongEntryFlag = 0x0f;
		pc2->L.CheckCode = total;
		memcpy(pc2->L.FileName0_4,p,10);
		p += 5;
		memcpy(pc2->L.FileName5_10,p,12);
		p += 6;
		memcpy(pc2->L.FileName11_12,p,4);
		p += 2;
		pc2--;
	}
	pc2[1].L.Property.bits.EndFlag = 1;
}
static void _print_str(const char* s,int len){
	int old = strlen(s);
	strncpy(pStr,s,len);
	if(old < len)
		memset(pStr+old,' ',len-old+1);
	pStr[len] = 0;
	printf("%s",pStr);
}

void DiskRW::Open(){
	_wsetlocale(LC_ALL,L"");// 仅需初始化一次
	hDisk = CreateFile(szDiscFile, GENERIC_READ|GENERIC_WRITE,  FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}
void DiskRW::Close(){
	CloseHandle(hDisk);
}
void DiskRW::Format(){
	frw.fat[0] = 0x0ffffff8;
	frw.fat[1] = 0xffffffff;
	frw.fat[2] = 0x0fffffff;
	memset(frw.fat,0,500);
	frw.Update(1);
	ReadCluster(sect0.RootStartCluster);
	memset(buf+32,0,16352);
	WriteCluster(sect0.RootStartCluster);
}
void DiskRW::CreateFileSystem(){
	ReadSector(buf,0,1);
	memcpy(&sect0,buf,sizeof(sect0));
	Fat1StartSector = sect0.rsvSectors;
	Fat2StartSector = sect0.rsvSectors + sect0.SectorsPerFAT_L;
	RootStartSector = Fat2StartSector + sect0.SectorsPerFAT_L;
	cata.name = "root";
	cata.offset = sect0.RootStartCluster;
	cata.len = 0;
	cata.isdir = 1;
	frw.Update(0);
	frw.FreeCluster = frw.next_free_fat(0);
}
void DiskRW::WriteCluster(int dest){
	WriteSector(buf,Cluster2Sector(dest),SPC);
}
void DiskRW::ReadCluster(int dest){
	if(dest != CurRecordCluster){
		CurRecordCluster = dest;
		ReadSector(buf,Cluster2Sector(dest),SPC);
	}
}

void FileRW::open(const char* name,int right){
	pCurrent = find(&drw.cata,name);
	if(NULL != pCurrent){
		CurFatInd = pCurrent->offset & FAT_MASK;
		CurFatPage =  pCurrent->offset >> FAT_BITS;
		Access = right;
		if(NULL != pParent && (_WRITE & Access || _REMOVE & Access)){
			RecordCluster = pParent->offset;
			RecordOffset = GetRecord(FIND_EXIST_RECORD,0);
		}
	}
}
void FileRW::close(){
	memset(&CurFatInd,0,20);
}
void FileRW::create(const char* name){
	char tmp[128],type = 0,len;

	if(NULL != find(&drw.cata,name))
		return;

	strcpy(tmp,name);
	len = strlen(tmp);
	if('\\' == tmp[len-1]){
		type = 1;
		tmp[len-1] = 0;
	}
	split_str(tmp,tmp+64,tmp,'\\');
	open(tmp,_WRITE);
	if(NULL != pCurrent){
		int si;
		RecordCluster = pCurrent->offset;
		drw.ReadCluster(RecordCluster);
		tt.sub.empty();
		tt.name = tmp + 64;
		if(0 == type){
			split_str(strbuf,pStr,tmp+64,'.');
			si = RecordCluster;
		}
		else{
			si = FreeCluster;
			setvar(FreeCluster,0xfffffff);
			FreeCluster = next_free_fat(FreeCluster+1);
			frw.Update(1);
		}
		if(8 < strlen(strbuf) || 3 < strlen(pStr) || (1 == type && 8 < strlen(tmp+64)))
			SetLongName(tmp+64,type);
		else
			SetShortName(tmp+64,type);
		SetRecord(si,0,type);
		SetOthers(si,0,type);
		drw.WriteCluster(RecordCluster);
	}
}
void FileRW::remove(const char* name){
	open(name,_REMOVE);
	if(NULL != pCurrent){
	}
}
void FileRW::read(void* p,int len){
	if(_READ & Access){
	}
}
void FileRW::write(void* p,int len){
	if(_WRITE & Access){
	}
}
int FileRW::GetRecord(int mode,int arg){
	int i = 0;
	drw.ReadCluster(RecordCluster);
	pc1 = (CATALOG *)drw.buf;
	if(pc1->S.Property.bits.SUB == 1)
		pc1 += 2;
	else
		pc1 += 1;
	while(pc1->S.FileName[0]){
		i = 0;
		if(pc1->L.Property.all != 0xe5){
			if(FIND_FREE_RECORD == mode)
				pc1++;
			else{
				if(pc1->L.LongEntryFlag != 0x0f)
					GetShortName();
				else
					GetLongName();
				if(READ_RECORD == mode)
					pCurrent->sub.push_back(tt);
				else{
					if(!_stricmp(pCurrent->name.data(),tt.name.data()))
						break;
				}
			}
		}
		else{
			pc1++;
			if(FIND_FREE_RECORD == mode){
				if(++i == arg)
					break;
			}
		}
	}
	return pc1 - (CATALOG *)drw.buf - i;
}
void FileRW::SetRecord(int offset,int len,int isdir){
	pc1->S.StartClusterH = offset >> 16;
	pc1->S.StartClusterL = offset & 0xffff;
	pc1->S.FileLengthInByte = len;
	if(0 == isdir)
		pc1->S.Property.bits.FI = 1;
	else
		pc1->S.Property.bits.SUB = 1;
	drw.WriteCluster(drw.CurRecordCluster);
}
Tree* FileRW::find(Tree* pt,const char* name){
	int old_len = strlen(name);
	char* p = pStr;
	strcpy(p,name);
	p[old_len+1] = 0;
	while(1){
		p = strchr(p+1,'\\');
		if(NULL == p)
			break;
		p[0] = 0;
	}
	return _find(pt,pStr);
}
Tree* FileRW::_find(Tree* pt,const char* name){
	if(0 == _stricmp(pt->name.data(),name) || ':' == name[1]){
		if(1 == pt->isdir){
			name += strlen(name)+1;
			if(0 == name[0])
				return pt;
			for(int i=0;i<pt->sub.size();i++){
				pCurrent = _find(&pt->sub[i],name);
				if(NULL != pCurrent){
					if(NULL == pParent)
						pParent = pt;
					return pCurrent;
				}
			}
		}
		else
			return pt;
	}
	return NULL;
}
DWORD FileRW::next_fat(){
	return getvar(CurFatInd);
}
DWORD FileRW::next_free_fat(int si){
	while(0 != getvar(si++));
	return si-1;
}
DWORD FileRW::getvar(int ind){
	int i = ind >> FAT_BITS;
	if(i != CurFatPage){
		CurFatPage = i;
		Update(0);
	}
	int v = fat[ind & FAT_MASK];
	if(v >= 0xffffff7 && v <= 0xfffffff)
		return -1;
	return v;
}
void FileRW::setvar(int ind,DWORD var){
	fat[ind & FAT_MASK] = var;
}
void FileRW::Update(int mode){
	if(1 == mode){
		WriteSector(fat,CurFatPage+drw.Fat1StartSector,1);
		WriteSector(fat,CurFatPage+drw.Fat2StartSector,1);
	}
	else{
		ReadSector(fat,CurFatPage+drw.Fat1StartSector,1);
	}
}

void ReadSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	ReadFile(hDisk, p, len<<9, &dwRealNumber, NULL);
}
void WriteSector(void* p,int si,int len){
	SetFilePointer(hDisk, si<<9, 0, FILE_BEGIN);
	DeviceIoControl(hDisk,FSCTL_LOCK_VOLUME,NULL,0,NULL,0,&dwRealNumber,NULL);
	WriteFile(hDisk, p, len<<9, &dwRealNumber, NULL);
	DeviceIoControl(hDisk,FSCTL_UNLOCK_VOLUME,NULL,0,NULL,0,&dwRealNumber,NULL);
}
void ReadDir(Tree* pt){
	if(1 == pt->isdir){
		frw.RecordCluster = pt->offset;
		frw.pCurrent = pt;
		frw.GetRecord(READ_RECORD,0);
	}
	int i,j = pt->sub.size();
	for(i=0;i<j;i++)
		ReadDir(&pt->sub[i]);
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
		ReadSector(drw.buf,pt->offset,1);
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
