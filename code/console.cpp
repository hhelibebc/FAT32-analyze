#include "FAT32.h"

int main(){
	drw.Open();
	drw.CreateFileSystem();
	ReadDir(&drw.cata);
	Travel(&drw.cata,Print,1);
	system("pause");
    return 0;
}
