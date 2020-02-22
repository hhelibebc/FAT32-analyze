#include "FAT32.h"

int main(){
	drw.Open();
	drw.CreateFileSystem();
	drw.ReadDir(&cata,drw.RootStartSector);
	drw.Travel(&cata,Print);
	system("pause");
    return 0;
}
