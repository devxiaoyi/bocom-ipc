#include "bocom_ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


typedef struct
{
    int a;
    char tmpMsg[30];
    int b;
} st_abc;

int main()
{
    int ret = 0;
    st_QUEUE_INFO queueInfo = {
        .queueName = "MyVideoChannel",
        .maxElementSize = 1000,
        .maxQueueSize = 12,
        .queueMode = Polling};
    Context queCtx = BOCOM_CreateQueue(&queueInfo);
    if (NULL == queCtx)
    {
        printf("BOCOM_CreateChannel failed\n");
        return -1;
    }
    printf("BOCOM_CreateChannel success\n");


#if 1

    int i = 0;
    char tmpMsg[30];
    while (1)
    {
        snprintf(tmpMsg, 30, "qwertyu123456_%d", i++);
        int reti = BOCOM_PublishQueue(queCtx, tmpMsg, 30);
        printf("main pub msg,rec:%s ret:%d \n",tmpMsg,reti);

        usleep(500*1000);
    }
    
#else

    int dataLen = 3 * 512 * 1024;
    void *tmpMsg = malloc(dataLen);
    if (tmpMsg == NULL)
    {
        printf("malloc is NULL\n");
        return -1;
    }

    FILE *fp = fopen("../test/1M.png", "r");
    if (fp == NULL)
    {
        printf("fopen is NULL\n");
        return -1;
    }
    fread(tmpMsg, 1, 1126447, fp);

    int reti = BOCOM_PublishQueue(queCtx, tmpMsg, 1126447);

    // snprintf(tmpMsg, 30, "qwertyu123456_%d", i++);
    // reti = BOCOM_PublishQueue(queCtx, chnInfo.channelName, abc.tmpMsg, sizeof(st_abc));

    printf("main pub msg, ret:%d \n", reti);
    
    sleep(5);

    free(tmpMsg);
    fclose(fp);

#endif

    return 0;
}
