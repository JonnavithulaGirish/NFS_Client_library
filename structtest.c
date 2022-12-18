#include <stdio.h>
#include <stdlib.h>
#include<string.h>
struct node{
    char* name;
    int x;
    int y;
    int z;
};

void func(struct node temp){
    printf("Hello World %p\n",temp.name);
}

int main()
{
    // char* a = "abc";
    struct node temp1;
    temp1.name = (char*)malloc(sizeof(char)*100);
    strcpy(temp1.name,"abc def");
    temp1.x = 1;
    temp1.y = 2;
    temp1.z = 3;
    func(temp1);
    printf("%d %d %d %p\n",temp1.x,temp1.y,temp1.z,temp1.name);
    

    return 0;
}
