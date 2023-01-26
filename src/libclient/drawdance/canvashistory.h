#ifndef DRAWDANCE_CANVASHISTORY_H
#define DRAWDANCE_CANVASHISTORY_H

#include <QMetaType>

struct DP_CanvasHistorySnapshot;
struct DP_CanvasHistoryEntry;
struct DP_ForkEntry;

namespace drawdance {

class CanvasHistorySnapshot final {
public:
	static CanvasHistorySnapshot null();
	static CanvasHistorySnapshot inc(DP_CanvasHistorySnapshot *chs);
	static CanvasHistorySnapshot noinc(DP_CanvasHistorySnapshot *chs);

    CanvasHistorySnapshot();
	CanvasHistorySnapshot(const CanvasHistorySnapshot &other);
	CanvasHistorySnapshot(CanvasHistorySnapshot &&other);

	CanvasHistorySnapshot &operator=(const CanvasHistorySnapshot &other);
	CanvasHistorySnapshot &operator=(CanvasHistorySnapshot &&other);

	~CanvasHistorySnapshot();

    bool isNull() const;

    int historyOffset() const;
    int historyCount() const;
    const DP_CanvasHistoryEntry *historyEntryAt(int index) const;

    int forkStart() const;
    int forkFallbehind() const;
    int forkCount() const;
    const DP_ForkEntry *forkEntryAt(int index) const;

private:
	explicit CanvasHistorySnapshot(DP_CanvasHistorySnapshot *chs);

	DP_CanvasHistorySnapshot *m_data;
};

}

// qRegisterMetaType is in the DumpPlaybackDialog.
Q_DECLARE_METATYPE(drawdance::CanvasHistorySnapshot)

#endif
