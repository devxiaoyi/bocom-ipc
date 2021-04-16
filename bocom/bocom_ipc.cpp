#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <cstdlib> //std::system
#include <cstddef>
#include <string>
#include <utility>
#include <iostream>
#include "bocom_ipc.h"

#define LOG(tag, msg)                                        \
    do                                                       \
    {                                                        \
        std::cout << "[" << tag << "] " << msg << std::endl; \
    } while (false);

#define BOCOM_PRIV_NAME_LEN 128
constexpr auto BOCOM_PRIV_HOLD_SIZE = 2048;

using namespace boost::interprocess;

using RwlockType = boost::interprocess::interprocess_upgradable_mutex;
using CondPubType = boost::interprocess::interprocess_condition_any;
using BcomMsgType = std::pair<int, managed_shared_memory::handle_t>;
using QueSizeType = std::pair<uint32_t, uint32_t>;      //(maxQueueSize,maxElementSize)

struct QueueContext
{
    uint64_t index = 0UL;
    managed_shared_memory *segment = nullptr;
};

typedef struct
{
    managed_shared_memory::handle_t itemHandle;
    uint32_t itemLength;
    uint64_t itemIndex;
} QueMsgType;

//Define an STL compatible allocator of ints that allocates from the managed_shared_memory.
//This allocator will allow placing containers in the segment
using ShmemAllocator = allocator<QueMsgType, managed_shared_memory::segment_manager>;
//Alias a deque that uses the previous STL-like allocator so that allocates
//its values from the segment
using BcomDequeType = deque<QueMsgType, ShmemAllocator>;

typedef managed_shared_memory::const_named_iterator const_named_it;

static void *AllocInShmem(managed_shared_memory *segment, int length)
{
    managed_shared_memory::size_type free_memory = segment->get_free_memory();
    void *shptr = segment->allocate(length);
    if (nullptr == shptr)
    {
        LOG("BOCOM_AllocShmem", "alloc failed!");
        return nullptr;
    }
    //Check invariant
    if (free_memory <= segment->get_free_memory())
    {
        LOG("BOCOM_AllocShmem", "failed , there has no free memory!");
        return nullptr;
    }
    return shptr;
}

static void *GetAddrInShmem(managed_shared_memory *segment, char *objName)
{
    if (nullptr == segment->find<BcomMsgType>(objName).first)
    {
        LOG("BOCOM_GetAddrInShmem", "cannot find objName objectName !");
        return nullptr;
    }
    return segment->get_address_from_handle(segment->find<BcomMsgType>(objName).first->second);
}

static Context CreateChannel(st_CHANNAL_INFO *info)
{
    //Erase previous shared memory and schedule erasure on exit
    shared_memory_object::remove(info->channelName);

    //Construct managed shared memory
    managed_shared_memory *segment = new managed_shared_memory(create_only, info->channelName, (info->channelSize + 1024));

    LOG("BOCOM_CreateChannel", "SUCCESS!");

    return segment;
}

static ErrorCode ConstructObject(Context chnCtx, st_OBJECT_INFO *info)
{
    if (chnCtx == nullptr)
    {
        LOG("BOCOM_ConstructObject", "param is null !");
        return ComError;
    }
    managed_shared_memory *segment = static_cast<managed_shared_memory *>(chnCtx);

    try
    {
        char this_rwlock[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_RWLOCK_";
        strcat(this_rwlock, info->objectName);
        segment->construct<RwlockType>(this_rwlock)();

        char this_cond_pub[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_CONDPUB_";
        strcat(this_cond_pub, info->objectName);
        segment->construct<CondPubType>(this_cond_pub)();

        //Allocate a portion of the segment (raw memory)
        void *shptr = AllocInShmem(segment, info->objectSize);
        if (shptr == nullptr)
        {
            return MemLack;
        }
        //An handle from the base address can identify any byte of the shared
        //memory segment even if it is mapped in different base addresses
        managed_shared_memory::handle_t handle = segment->get_handle_from_address(shptr);

        //Create an handle of BcomMsgType in segment
        segment->construct<BcomMsgType>(info->objectName)(info->objectSize, handle);
    }
    catch (interprocess_exception &ex)
    {
        LOG("", ex.what());
        return ComError;
    }
    return Success;
}

static ErrorCode CheckObjectExist(Context chnCtx, char *objectName)
{
    managed_shared_memory *segment = static_cast<managed_shared_memory *>(chnCtx);
    const_named_it named_beg = segment->named_begin();
    const_named_it named_end = segment->named_end();

    for (; named_beg != named_end; ++named_beg)
    {
        //A pointer to the name of the named object
        const managed_shared_memory::char_type *name = named_beg->name();
        //The length of the name
        //std::size_t name_len = named_beg->name_length();

        if (0 == std::strcmp(objectName, name))
        {
            return Success;
        }
    }
    return ComError;
}

static ErrorCode DestroyObject(Context chnCtx, st_OBJECT_INFO *info)
{
    if (chnCtx == nullptr)
    {
        LOG("BOCOM_DestroyObject", "param is null !");
        return ComError;
    }
    managed_shared_memory *segment = static_cast<managed_shared_memory *>(chnCtx);

    try
    {
        //Dealloc objectName's memory
        if (nullptr == segment->find<BcomMsgType>(info->objectName).first)
        {
            LOG("BOCOM_DestroyObject", "cannot find objectName !");
            return ComError;
        }
        void *msg = segment->get_address_from_handle(segment->find<BcomMsgType>(info->objectName).first->second);
        if (msg != nullptr)
        {
            segment->deallocate(msg);
        }
        else
        {
            LOG("BOCOM_DestroyObject", "deallocate's memory is null !");
            return ComError;
        }
        //Destroy the segment
        segment->destroy<BcomMsgType>(info->objectName);

        //Dealloc object rwlock's memory
        char this_rwlock[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_RWLOCK_";
        strcat(this_rwlock, info->objectName);
        //Destroy the segment
        segment->destroy<RwlockType>(this_rwlock);

        //Dealloc object condition's memory
        char this_cond_pub[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_CONDPUB_";
        strcat(this_cond_pub, info->objectName);
        segment->destroy<CondPubType>(this_cond_pub);
    }
    catch (interprocess_exception &ex)
    {
        LOG("", ex.what());
        return ComError;
    }
    return Success;
}

static ErrorCode Publish(Context chnCtx, char *objectName, void *value, int valueLength, int flags)
{
    if (chnCtx == nullptr || value == nullptr)
    {
        LOG("BOCOM_Publish", "param is null !");
        return ComError;
    }
    managed_shared_memory *segment = static_cast<managed_shared_memory *>(chnCtx);

    try
    {
        char this_rwlock[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_RWLOCK_";
        strcat(this_rwlock, objectName);
        RwlockType *rwlock = segment->find<RwlockType>(this_rwlock).first;
        if (nullptr == rwlock)
        {
            LOG("BOCOM_Publish", "data is nullptr !");
            return ComError;
        }

        if (0 == flags)
        {
            //Non-blocking
            return ComError;
        }
        else if (1 == flags || 2 == flags)
        {
            scoped_lock<interprocess_upgradable_mutex> lock(*rwlock);
            void *msg_data = GetAddrInShmem(segment, objectName);
            if (nullptr == msg_data)
            {
                LOG("BOCOM_Publish", "data is nullptr !");
                return ComError;
            }
            memcpy(msg_data, value, valueLength);
        }
        else
        {
            LOG("BOCOM_Publish", "flags is illegal !");
            return ComError;
        }

        if (2 == flags)
        {
            char this_cond_pub[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_CONDPUB_";
            strcat(this_cond_pub, objectName);
            CondPubType *cond_pub = segment->find<CondPubType>(this_cond_pub).first;
            cond_pub->notify_all();
        }
    }
    catch (interprocess_exception &ex)
    {
        LOG("", ex.what());
        return ComError;
    }
    return Success;
}

static Context JoinChannel(char *channelName)
{
    managed_shared_memory *segment = new managed_shared_memory(open_only, channelName);

    LOG("BOCOM_JoinChannel", "SUCCESS!");

    return static_cast<Context>(segment);
}

static ErrorCode Retrieve(Context chnCtx, char *objectName, void *outPutValue, int valueLength, int flags)
{
    if (chnCtx == nullptr || outPutValue == nullptr)
    {
        LOG("BOCOM_Retrieve", "param is null !");
        return ComError;
    }

    managed_shared_memory *segment = static_cast<managed_shared_memory *>(chnCtx);

    try
    {
        char this_rwlock[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_RWLOCK_";
        strcat(this_rwlock, objectName);
        RwlockType *rwlock = segment->find<RwlockType>(this_rwlock).first;
        if (nullptr == rwlock)
        {
            LOG("BOCOM_Retrieve", "data is null !");
            return ComError;
        }

        if (0 == flags)
        {
            //no-blocking
            return ComError;
        }
        else if (1 == flags)
        {
            sharable_lock<interprocess_upgradable_mutex> lock(*rwlock);
            void *msg = GetAddrInShmem(segment, objectName);
            if (msg != nullptr)
            {
                int msgLen = segment->find<BcomMsgType>(objectName).first->first;
                int minLen = std::min(valueLength, msgLen);
                memcpy(outPutValue, msg, minLen);
            }
            else
            {
                LOG("BOCOM_Retrieve", "data is null !");
                return ComError;
            }
        }
        else if (2 == flags)
        {
            sharable_lock<interprocess_upgradable_mutex> lock(*rwlock);

            char this_cond_pub[BOCOM_PRIV_NAME_LEN] = "BOCOM_PRIV_CONDPUB_";
            strcat(this_cond_pub, objectName);
            CondPubType *cond_pub = segment->find<CondPubType>(this_cond_pub).first;
            cond_pub->wait(lock);

            void *msg = GetAddrInShmem(segment, objectName);
            if (msg != nullptr)
            {
                int msgLen = segment->find<BcomMsgType>(objectName).first->first;
                int minLen = std::min(valueLength, msgLen);
                memcpy(outPutValue, msg, minLen);
            }
            else
            {
                LOG("BOCOM_Retrieve", "data is null !");
                return ComError;
            }
        }
        else
        {
            LOG("BOCOM_Retrieve", "flags is illegal !");
            return ComError;
        }
    }
    catch (interprocess_exception &ex)
    {
        LOG("", ex.what());
        return ComError;
    }
    return Success;
}

static QueueContext* CreateQueue(const st_QUEUE_INFO *info)
{
    //Erase previous shared memory and schedule erasure on exit
    shared_memory_object::remove(info->queueName);

    auto *context = new QueueContext;
    // TODO: the allocated memory is not sufficient as we don't cout the meta data.
    context->segment = new managed_shared_memory(create_only, info->queueName, (info->maxElementSize * info->maxQueueSize) + BOCOM_PRIV_HOLD_SIZE);
    managed_shared_memory *segment = context->segment;
    try
    {
        //Construct object management deque
        const ShmemAllocator alloc_inst(segment->get_segment_manager());
        //Construct a deque in shared memory with argument alloc_inst
        segment->construct<BcomDequeType>(info->queueName)(alloc_inst);
        //Construct for save queue size info (maxQueueSize,maxElementSize)
        segment->construct<QueSizeType>("BOCOM_PRIV_QUEUE_SIZE")(info->maxQueueSize, info->maxElementSize);
        //Construct for save queue index
        segment->construct<uint64_t>("BOCOM_PRIV_QUEUE_INDEX")(0);
        //Construct for rwlock
        segment->construct<RwlockType>("BOCOM_PRIV_RWLOCK_QUEUE")();
        //Construct for condition
        if(info->queueMode == Notify)
        {
            segment->construct<CondPubType>("BOCOM_PRIV_COND_QUEUE")();
        }
    }
    catch (interprocess_exception &ex)
    {
        LOG("CreateQueue", ex.what());
        return nullptr;
    }

    LOG("BOCOM_CreateQueue", "SUCCESS!");

    return context;
}

static ErrorCode DestroyQueue(QueueContext *context)
{
    if (context == nullptr || context->segment == nullptr)
    {
        LOG("BOCOM_DestroyQueue", "param is null !");
        return ComError;
    }
    managed_shared_memory *segment = context->segment;

    const char *queueName = nullptr;
    const_named_it named_beg = segment->named_begin();
    const_named_it named_end = segment->named_end();
    for (; named_beg != named_end; ++named_beg)
    {
        //A pointer to the name of the named object
        const managed_shared_memory::char_type *name = named_beg->name();
        if (std::strncmp("BOCOM_PRIV_", name, 10))
        {
            queueName = name;
            break;
        }
    }
    if (queueName == nullptr)
    {
        LOG("BOCOM_Destroy", "queueName is null");
        return Invalid;
    }

    BcomDequeType *this_deque = segment->find<BcomDequeType>(queueName).first;
    while (this_deque->size() > 0)
    {
        QueMsgType queItem = this_deque->front();
        void *shptr = segment->get_address_from_handle(queItem.itemHandle);
        if (nullptr != shptr)
        {
            segment->deallocate(shptr);
        }
        this_deque->pop_front();
    }
    segment->destroy<BcomDequeType>(queueName);

    if (segment->find<RwlockType>("BOCOM_PRIV_RWLOCK_QUEUE").first)
    {
        segment->destroy<RwlockType>("BOCOM_PRIV_RWLOCK_QUEUE");
    }
    if (segment->find<CondPubType>("BOCOM_PRIV_COND_QUEUE").first)
    {
        segment->destroy<CondPubType>("BOCOM_PRIV_COND_QUEUE");
    }

    shared_memory_object::remove(queueName);

    if (nullptr != context->segment)
    {
        delete context->segment;
        context->segment = nullptr;
    }
    if (nullptr != context)
    {
        delete context;
        context = nullptr;
    }
    return Success;
}

static ErrorCode PublishQueue(QueueContext *context, const void *value, unsigned int valueLength)
{
    if (context == nullptr || value == nullptr || context->segment == nullptr)
    {
        LOG("BOCOM_Publish", "param is null !");
        return ComError;
    }
    managed_shared_memory *segment = context->segment;

    try
    {
        const char* queueName = nullptr;

        const_named_it named_beg = segment->named_begin();
        const_named_it named_end = segment->named_end();
        for (; named_beg != named_end; ++named_beg)
        {
            //A pointer to the name of the named object
            const managed_shared_memory::char_type *name = named_beg->name();
            if (std::strncmp("BOCOM_PRIV_", name,10))
            {
                queueName = name;
                break;
            }
        }
        if(queueName == nullptr)
        {
            LOG("BOCOM_Publish", "queueName is null");
            return Invalid;
        }

        RwlockType *rwlock = segment->find<RwlockType>("BOCOM_PRIV_RWLOCK_QUEUE").first;
        if (nullptr == rwlock)
        {
            LOG("BOCOM_Publish", "data is nullptr !");
            return ComError;
        }
        scoped_lock<interprocess_upgradable_mutex> lock(*rwlock);

        BcomDequeType *this_deque = segment->find<BcomDequeType>(queueName).first;

        uint32_t maxQueueSize = segment->find<QueSizeType>("BOCOM_PRIV_QUEUE_SIZE").first->first;
        uint32_t maxElementSize = segment->find<QueSizeType>("BOCOM_PRIV_QUEUE_SIZE").first->second;
        if(valueLength > maxElementSize)
        {
            LOG("BOCOM_Publish", "valueLength is larger than maxElementSize !");
            return Invalid;
        }
        void *shptr = NULL;
        if (this_deque->size() < maxQueueSize)
        {
            shptr = AllocInShmem(segment, maxElementSize);
            if (shptr == nullptr)
            {
                LOG("BOCOM_Publish", "data alloc shptr is nullptr !");
                return ComError;
            }
        }
        else
        {
            QueMsgType queItem = this_deque->front();
            shptr = segment->get_address_from_handle(queItem.itemHandle);
            if (nullptr == shptr)
            {
                LOG("BOCOM_Publish", "data shptr is nullptr !");
                return ComError;
            }
            if (queItem.itemLength > 0)
            {
                std::memset(shptr,0,queItem.itemLength);
            }
            this_deque->pop_front();
        }
        if (valueLength > 0 && shptr != nullptr)
        {
            std::memcpy(shptr, value, valueLength);
        }
        else
        {
            LOG("BOCOM_Publish", "copy value failed !");
            return ComError;
        }
        managed_shared_memory::handle_t handle = segment->get_handle_from_address(shptr);
        QueMsgType tmpQueMsg = {
            .itemHandle = handle,
            .itemLength = valueLength,
            .itemIndex = (*(segment->find<uint64_t>("BOCOM_PRIV_QUEUE_INDEX").first))++};
        this_deque->push_back(tmpQueMsg);

        CondPubType *cond_pub = segment->find<CondPubType>("BOCOM_PRIV_COND_QUEUE").first;
        if(nullptr != cond_pub)
        {
            cond_pub->notify_all();
        }
    }
    catch (interprocess_exception &ex)
    {
        LOG("PublishQueue", ex.what());
        return ComError;
    }
    return Success;
}

static QueueContext* JoinQueue(const char *queueName)
{
    auto *context = new QueueContext;
    try
    {
        context->segment = new managed_shared_memory(open_only, queueName);
    }
    catch (interprocess_exception &ex)
    {
        LOG("JoinQueue", ex.what());
        return nullptr;
    }

    LOG("BOCOM_JoinQueue", "SUCCESS!");

    return context;
}

static ErrorCode QuitQueue(QueueContext *context)
{
    if (nullptr != context->segment)
    {
        delete context->segment;
        context->segment = nullptr;
    }
    else
    {
        LOG("BOCOM_QuitQueue", "segment is null !");
        return ComError;
    }

    if (nullptr != context)
    {
        delete context;
        context = nullptr;
    }
    else
    {
        LOG("BOCOM_QuitQueue", "context is null !");
        return ComError;
    }
    return Success;
}

static ErrorCode RetrieveQueue(QueueContext* context, void *outputValue, unsigned int *valueLength)
{
    if (context == nullptr || outputValue == nullptr || context->segment == nullptr)
    {
        LOG("BOCOM_Retrieve", "param is null !");
        return ComError;
    }

    managed_shared_memory *segment = context->segment;

    try
    {
        const char* queueName = nullptr;

        const_named_it named_beg = segment->named_begin();
        const_named_it named_end = segment->named_end();
        for (; named_beg != named_end; ++named_beg)
        {
            //A pointer to the name of the named object
            const managed_shared_memory::char_type *name = named_beg->name();
            if (std::strncmp("BOCOM_PRIV_", name,10))
            {
                queueName = name;
                break;
            }
        }
        if(queueName == nullptr)
        {
            LOG("BOCOM_Publish", "queueName is null");
            return Invalid;
        }

        RwlockType *rwlock = segment->find<RwlockType>("BOCOM_PRIV_RWLOCK_QUEUE").first;
        if (nullptr == rwlock)
        {
            LOG("BOCOM_Retrieve", "data is null !");
            return ComError;
        }
        sharable_lock<interprocess_upgradable_mutex> lock(*rwlock);

        CondPubType *cond_pub = segment->find<CondPubType>("BOCOM_PRIV_COND_QUEUE").first;
        if(nullptr != cond_pub)
        {
            cond_pub->wait(lock);
        }

        BcomDequeType *this_deque = segment->find<BcomDequeType>(queueName).first;

        if(this_deque->empty())
        {
            return NoData;
        }
        
        QueMsgType queItem = this_deque->front();
        //Set the initial value at the first call
        if (context->index == 0)
        {
            context->index = queItem.itemIndex;
        }
        //If the last traversal to the end of the queue returns no data
        if ((context->index > queItem.itemIndex) && (context->index - queItem.itemIndex >= this_deque->size()))
        {
            return NoData;
        }

        for (BcomDequeType::iterator itor = this_deque->begin(); itor != this_deque->end(); ++itor)
        {
            if (itor->itemIndex == context->index)
            {
                //normal
                void *msg = segment->get_address_from_handle(itor->itemHandle);
                const int msgLen = itor->itemLength;
                if(msgLen > 0)
                {
                    std::memcpy(outputValue, msg, msgLen);
                }
                if(NULL != valueLength)
                {
                    *valueLength = msgLen;
                }

                context->index++;
                return Success;
            }
            else if (itor->itemIndex < context->index)
            {
                //retreive faster
                continue;
            }
            else
            {
                //retreive slower
                void *msg = segment->get_address_from_handle(itor->itemHandle);
                const int msgLen = itor->itemLength;
                if(msgLen > 0)
                {
                    std::memcpy(outputValue, msg, msgLen);
                }
                if(NULL != valueLength)
                {
                    *valueLength = msgLen;
                }

                context->index = itor->itemIndex + 1;
                return DataLost;
            }
        }
    }
    catch (interprocess_exception &ex)
    {
        LOG("RetrieveQueue", ex.what());
        return ComError;
    }
    return Success;
}

#ifdef __cplusplus
extern "C"
{
#endif

Context BOCOM_CreateChannel(st_CHANNAL_INFO *info)
{
    return CreateChannel(info);
}

ErrorCode BOCOM_ConstructObject(Context chnCtx, st_OBJECT_INFO *info)
{
    return ConstructObject(chnCtx, info);
}

ErrorCode BOCOM_CheckObjectExist(Context chnCtx, char *objectName)
{
    return CheckObjectExist(chnCtx, objectName);
}

ErrorCode BOCOM_DestroyObject(Context chnCtx, st_OBJECT_INFO *info)
{
    return DestroyObject(chnCtx, info);
}

ErrorCode BOCOM_Publish(Context chnCtx, char *objectName, void *value, int valueLength, int flags)
{
    return Publish(chnCtx, objectName, value, valueLength, flags);
}

Context BOCOM_JoinChannel(char *channelName)
{
    return JoinChannel(channelName);
}

ErrorCode BOCOM_Retrieve(Context chnCtx, char *objectName, void *outPutValue, int valueLength, int flags)
{
    return Retrieve(chnCtx, objectName, outPutValue, valueLength, flags);
}

Context BOCOM_CreateQueue(const st_QUEUE_INFO *info)
{
    return static_cast<Context>(CreateQueue(info));
}

ErrorCode BOCOM_DestroyQueue(Context context)
{
    return DestroyQueue(static_cast<QueueContext*>(context));
}

ErrorCode BOCOM_PublishQueue(Context context, const void *value, unsigned int valueLength)
{
    return PublishQueue(static_cast<QueueContext*>(context), value, valueLength);
}

Context BOCOM_JoinQueue(const char *queueName)
{
    return static_cast<Context>(JoinQueue(queueName));
}

ErrorCode BOCOM_QuitQueue(Context context)
{
    return QuitQueue(static_cast<QueueContext*>(context));
}

ErrorCode BOCOM_RetrieveQueue(Context context, void *outputValue, unsigned int *valueLength)
{
    return RetrieveQueue(static_cast<QueueContext*>(context), outputValue, valueLength);
}

#ifdef __cplusplus
};
#endif
