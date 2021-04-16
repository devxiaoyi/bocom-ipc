#include "bocom_ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#if 1

typedef struct
{
    int a;
    char tmpMsg[30];
    int b;
} st_abc;

int main()
{
    Context chnCtx = BOCOM_JoinChannel("MyVideoChannel");

    printf("BOCOM_JoinChannel success !\n");

    void *pt = malloc(sizeof(st_abc));

    st_abc abc;

    while (1)
    {
        memset(pt, 0, sizeof(st_abc));
        ErrorCode reti = BOCOM_Retrieve(chnCtx, "IFrameBuff", pt, sizeof(st_abc), 2);
        printf("main rec IFrameBuff rec:%s ret:%d \n", ((st_abc *)pt)->tmpMsg, reti);
        //usleep(3 * 1000);

        ErrorCode retp = BOCOM_Retrieve(chnCtx, "PFrameBuff", &abc, sizeof(st_abc), 2);
        printf("main rec PFrameBuff rec:%s ret:%d \n", abc.tmpMsg, retp);
        usleep(3 * 1000);
    }

    free(pt);

    return 0;
}

//for test big-data
#else

#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

long long usedTime = 0;
long long waitTime = 0;
long long startTime = 0;
long long endTime = 0;
long long usedTimeMax = 0;
long long waitTimeMax = 0;

int i = 0;

void signalHandler(int signum)
{
    printf("average waitTime : %lld\n", waitTime / (i));
    printf("average usedTime : %lld\n", usedTime / (i));
    printf("usedTimeMax : %lld\n", usedTimeMax);
    printf("waitTimeMax : %lld\n", waitTimeMax);

    exit(signum);
}

static long long util_getCurrentTimeUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main()
{
    Context chnCtx = BOCOM_JoinChannel("MyVideoChannel");

    printf("BOCOM_JoinChannel success !\n");

    int dataLen = 5 * 1024 * 1024;
    void *tmpMsg = malloc(dataLen);
    if (tmpMsg == NULL)
    {
        printf("malloc is NULL\n");
        return -1;
    }

    signal(SIGINT, signalHandler);

    while (1)
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

        ErrorCode reti = BOCOM_Retrieve(chnCtx, "IFrameBuff", tmpMsg, 2);
        if (reti != 0)
        {
            printf("BOCOM_Retrieve error %d\n", reti);
        }
        endTime = util_getCurrentTimeUs();
        long long tmp = endTime - startTime;
        if (usedTimeMax < tmp)
        {
            usedTimeMax = tmp;
        }
        usedTime = usedTime + tmp;

        i++;
    }

    if (tmpMsg != NULL)
    {
        free(tmpMsg);
    }

    return 0;
}

#endif
