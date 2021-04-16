#ifndef BOCOM_IPC_H
#define BOCOM_IPC_H

/*  Block diagram of channels and objects 
 * 
 * |------- channel --------|
 * |    |--- object_1 ---|  |
 * |    |----------------|  |
 * |                        |
 * |    |--- object_2 ---|  |
 * |    |----------------|  |
 * |------------------------|
 */

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct CHANNAL_INFO {
    char *channelName;
    int  channelSize;
} st_CHANNAL_INFO;

typedef struct OBJECT_INFO {
    char *objectName;
    int  objectSize;
} st_OBJECT_INFO;

typedef enum QueueMode {
    Polling = 0,
    Notify  = 1,
} QueueMode;

typedef struct QUEUE_INFO {
    char *queueName;
    int  maxElementSize;
    int  maxQueueSize;
    QueueMode queueMode;     //0:polling  1:notify
} st_QUEUE_INFO;

typedef enum ErrorCode {
    Success     = 0,
    ComError    = -1,
    Invalid     = -2,
    MemLack     = -3,
    NoData      = -4,
    DataLost    = -5
} ErrorCode;

typedef void* Context;

/* brief:  Create a channel before use. Then you can join it by channel-name in other processes
 *          Since some shared memory is occupied internally, you must apply for a larger memory
 *           (It depends on the number of objects you will use),
 *          otherwise "boost::interprocess::bad_alloc" will appear
 * param:  channel info
 * return: channel context
 */
Context BOCOM_CreateChannel(st_CHANNAL_INFO *info);

/* brief:  Construct an object in the channel. Similarly, you can find it by the object-name in other processes
 * param:  1.channel context   2.object info
 * return: ErrorCode (NOTE: determine the return value before proceeding to the next step)
 */
ErrorCode BOCOM_ConstructObject(Context chnCtx,st_OBJECT_INFO *info);

/* brief:  Determine whether the object exists in the channel
 * param:  1.channel context   2.object name
 * return: ErrorCode (NOTE: Success means existence)
 */
ErrorCode BOCOM_CheckObjectExist(Context chnCtx, char *objectName);

/* brief:  When you don't use the object, destroy it from the channel
 * param:  1.channel context   2.object info
 * return: ErrorCode
 */
ErrorCode BOCOM_DestroyObject(Context chnCtx,st_OBJECT_INFO *info);

/* brief:  Publish the value(data) to object
 * param:  1.object context   2.value  3.valueLength  4.flags(for blocking: 0 non-blocking  1 blocking mode  2 condition(send first))
 * return: ErrorCode
 */
ErrorCode BOCOM_Publish(Context chnCtx,char* objectName,void* value,int valueLength,int flags);


/* brief:  Other processes can join the channel in order to obtain objects
 * param:  channel name
 * return: channel context
 */
Context BOCOM_JoinChannel(char *channelName);

/* brief:  Get data from the previously constructed object
 * param:  1.channel context   2.object name  3.output value  4.flags(for blocking: 0 non-blocking  1 blocking mode  2 condition(Receive after sending))
 * return: ErrorCode
 */
ErrorCode BOCOM_Retrieve(Context chnCtx, char* objectName, void* outPutValue, int valueLength, int flags);


/* brief:  Create a data queue. Then you can join it by queue-name in other processes
 * param:  queue info: Include queueName maxElementSize maxQueueSize queueMode
 * return: queue context
 */
Context BOCOM_CreateQueue(const st_QUEUE_INFO *info);

/* brief:  destroy the queue that created before
 * param:  queue context
 * return: ErrorCode
 */
ErrorCode BOCOM_DestroyQueue(Context context);

/* brief:  Publish the value(data) to queue
 * param:  1.queue context   2.value  3.valueLength
 * return: ErrorCode
 */
ErrorCode BOCOM_PublishQueue(Context context, const void *value, unsigned int valueLength);

/* brief:  Other processes can join the queue in order to obtain objects
 * param:  queue name
 * return: queue context
 */
Context BOCOM_JoinQueue(const char *queueName);

/* brief:  Quit the queue that joined before
 * param:  queue context
 * return: ErrorCode
 */
ErrorCode BOCOM_QuitQueue(Context context);

/* brief:  Get data from the previously joined queue
 * param:  1.queue context  2.output value  3.length of the output value
 * return: ErrorCode
 */
ErrorCode BOCOM_RetrieveQueue(Context context, void *value, unsigned int *valueLength);

#ifdef __cplusplus
};
#endif

#endif
