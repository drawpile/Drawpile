#include "snapshotqueue.h"

#include <QDateTime>

namespace drawdance {

namespace {

struct GetSnapshotContext {
    SnapshotQueue::GetSnapshotsFn get;
};

}

SnapshotQueue::SnapshotQueue(size_t maxCount, long long minDelayMs)
    : m_data(DP_snapshot_queue_new(maxCount, minDelayMs, SnapshotQueue::getTimestampMs, nullptr))
{
}

SnapshotQueue::~SnapshotQueue()
{
    DP_snapshot_queue_free(m_data);
}

DP_SnapshotQueue *SnapshotQueue::get()
{
    return m_data;
}

void SnapshotQueue::getSnapshotsWith(GetSnapshotsFn get) const
{
    DP_snapshot_queue_get_with(m_data, &SnapshotQueue::getSnapshots, &get);
}

long long SnapshotQueue::getTimestampMs(void *)
{
    return QDateTime::currentMSecsSinceEpoch();
}

void SnapshotQueue::getSnapshots(
    void *user, DP_SnapshotQueue *sq, size_t count, DP_SnapshotAtFn at)
{
    (*static_cast<GetSnapshotsFn *>(user))(count, [&](int index) {
        return at(sq, index);
    });
}

}
