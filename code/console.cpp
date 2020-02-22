#include "FAT32.h"

int main(){
	drw.Open();
	drw.CreateFileSystem();
	Travel(&cata,ReadDir,1);
	Travel(&cata,Print,1);
	system("pause");
    return 0;
}
