#include "bocom_ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int a;
    char tmpMsg[30];
    int b;
} st_abc;

int main()
{
    Context queCtx = BOCOM_JoinQueue("MyVideoChannel");

    if(NULL == queCtx)
    {
        printf("BOCOM_JoinChannel failed !\n");
        return -1;
    }
    printf("BOCOM_JoinChannel success !\n");

    unsigned int recLen = 0;  //received data length

#if 1
    int i = 0;
    void *tmpMsg = malloc(100);
    if (tmpMsg == NULL)
    {
        printf("malloc is NULL\n");
        return -1;
    }
    int reti = 0;

    while (1)
    {
        memset(tmpMsg, 0, 100);
        reti = BOCOM_RetrieveQueue(queCtx, tmpMsg, &recLen);
        printf("main rec IFrameBuff rec:%s len:%d ret:%d \n",tmpMsg,recLen,reti);

        usleep(100*1000);
    }

#else

    int dataLen = 3 * 512 * 1024;
    void *tmpMsg = malloc(dataLen);
    if (tmpMsg == NULL)
    {
        printf("malloc is NULL\n");
        return -1;
    }

    memset(tmpMsg, 0, dataLen);

    ErrorCode reti = BOCOM_RetrieveQueue(queCtx, tmpMsg, &recLen);
    printf("main rec IFrameBuff rec:%s ret:%d \n", tmpMsg, reti);

    FILE *fp = fopen("../test/1M-img_cp.png", "w+");
    if (fp == NULL)
    {
        printf("fopen is NULL\n");
        return -1;
    }
    fwrite(tmpMsg, 1, recLen, fp);

    fclose(fp);
    free(tmpMsg);
#endif

    return 0;
}
