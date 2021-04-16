
#include "bocom_ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if 1

typedef struct
{
    int a;
    char tmpMsg[30];
    int b;
} st_abc;

int main()
{
    int ret = 0;

    st_CHANNAL_INFO chnInfo = {
        .channelName = "MyVideoChannel",
        .channelSize = 2000};
    Context chnCtx = BOCOM_CreateChannel(&chnInfo);
    if(NULL == chnCtx)
    {
        printf("BOCOM_CreateChannel failed\n");
        return -1;
    }

    printf("BOCOM_CreateChannel success\n");

    st_OBJECT_INFO ObjInfoI = {
        .objectName = "IFrameBuff",
        .objectSize = 1000};
    ret = BOCOM_ConstructObject(chnCtx, &ObjInfoI);

    printf("BOCOM_ConstructObject I ret:%d\n",ret);

    st_OBJECT_INFO ObjInfoP = {
        .objectName = "PFrameBuff",
        .objectSize = 100};
    ret = BOCOM_ConstructObject(chnCtx, &ObjInfoP);

    printf("BOCOM_ConstructObject P ret:%d\n",ret);

    st_OBJECT_INFO ObjInfoV = {
        .objectName = "VFrameBuff",
        .objectSize = 100};
    ret = BOCOM_ConstructObject(chnCtx, &ObjInfoV);

    printf("BOCOM_ConstructObject V ret:%d\n",ret);

    st_abc abc;
    abc.a = 12;
    abc.b = 32;

    int i = 0;

    ret = BOCOM_CheckObjectExist(chnCtx,"IFrameBuff");
    printf("CheckObjectExist ret:%d\n",ret);

    while (1)
    {
        snprintf(abc.tmpMsg, 30, "qwertyu123456_%d", i++);
        ErrorCode reti = BOCOM_Publish(chnCtx, ObjInfoI.objectName, &abc, sizeof(st_abc), 2);
        printf("main pub I msg,rec:%s ret:%d \n", abc.tmpMsg, reti);

        ErrorCode retp = BOCOM_Publish(chnCtx, ObjInfoP.objectName, &abc, sizeof(st_abc), 2);
        printf("main pub P msg,rec:%s ret:%d \n", abc.tmpMsg, retp);

        usleep(10 * 1000);
    }

    return 0;
}

//for test big-data
#else

#include <stdlib.h>
#include <sys/time.h>

static long long util_getCurrentTimeUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main()
{
    st_CHANNAL_INFO chnInfo = {
        .channelName = "MyVideoChannel",
        .channelSize = 6 * 1024 * 1024};
    Context chnCtx = BOCOM_CreateChannel(&chnInfo);

    printf("BOCOM_CreateChannel success\n");

    st_OBJECT_INFO ObjInfoI = {
        .objectName = "IFrameBuff",
        .objectSize = 5 * 1024 * 1024};
    BOCOM_ConstructObject(chnCtx, &ObjInfoI);

    printf("BOCOM_ConstructObject I success\n");

    int dataLen = 5 * 1024 * 1024;
    void *tmpMsg = malloc(dataLen);
    if (tmpMsg == NULL)
    {
        printf("malloc is NULL\n");
        return -1;
    }

    FILE *fp = fopen("/mnt/4k-img.jpg", "r");
    if (fp == NULL)
    {
        printf("fopen is NULL\n");
        return -1;
    }
    fread(tmpMsg, 1, 5114738, fp);

    int i = 0;

    long long usedTime = 0;
    long long waitTime = 0;
    long long startTime = 0;
    long long endTime = 0;
    long long usedTimeMax = 0;
    long long waitTimeMax = 0;

    int nums = 2000;
    while (i < nums)
    {

        startTime = util_getCurrentTimeUs();
        if (0 != endTime)
        {

            long long tmp = (startTime - endTime);
            if (waitTimeMax < tmp)
            {
                waitTimeMax = tmp;
            }
            waitTime = waitTime + tmp;
        }

        ErrorCode reti = BOCOM_Publish(chnCtx, ObjInfoI.objectName, tmpMsg, dataLen, 2);
        if (reti != 0)
        {
            printf("BOCOM_Publish error %d\n", reti);
        }

        endTime = util_getCurrentTimeUs();
        long long tmp = endTime - startTime;
        if (usedTimeMax < tmp)
        {
            usedTimeMax = tmp;
        }
        usedTime = usedTime + tmp;

        i++;
        //usleep(10 * 1000);
    }

    printf("average waitTime : %lld\n", waitTime / (nums - 1));
    printf("average usedTime : %lld\n", usedTime / (nums));
    printf("usedTimeMax : %lld\n", usedTimeMax);
    printf("waitTimeMax : %lld\n", waitTimeMax);

    if (tmpMsg != NULL)
    {
        free(tmpMsg);
    }
    fclose(fp);

    return 0;
}

#endif
