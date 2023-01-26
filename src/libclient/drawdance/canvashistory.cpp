extern "C" {
#include <dpengine/canvas_history.h>
}

#include "canvashistory.h"

namespace drawdance {

CanvasHistorySnapshot CanvasHistorySnapshot::null()
{
	return CanvasHistorySnapshot{nullptr};
}

CanvasHistorySnapshot CanvasHistorySnapshot::inc(DP_CanvasHistorySnapshot *chs)
{
	return CanvasHistorySnapshot{
		DP_canvas_history_snapshot_incref_nullable(chs)};
}

CanvasHistorySnapshot
CanvasHistorySnapshot::noinc(DP_CanvasHistorySnapshot *chs)
{
	return CanvasHistorySnapshot{chs};
}

CanvasHistorySnapshot::CanvasHistorySnapshot()
	: CanvasHistorySnapshot{nullptr}
{
}

CanvasHistorySnapshot::CanvasHistorySnapshot(const CanvasHistorySnapshot &other)
	: CanvasHistorySnapshot{
		  DP_canvas_history_snapshot_incref_nullable(other.m_data)}
{
}

CanvasHistorySnapshot::CanvasHistorySnapshot(CanvasHistorySnapshot &&other)
	: CanvasHistorySnapshot{other.m_data}
{
	other.m_data = nullptr;
}

CanvasHistorySnapshot &
CanvasHistorySnapshot::operator=(const CanvasHistorySnapshot &other)
{
	DP_canvas_history_snapshot_decref_nullable(m_data);
	m_data = DP_canvas_history_snapshot_incref_nullable(other.m_data);
	return *this;
}

CanvasHistorySnapshot &
CanvasHistorySnapshot::operator=(CanvasHistorySnapshot &&other)
{
	DP_canvas_history_snapshot_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

CanvasHistorySnapshot::~CanvasHistorySnapshot()
{
	DP_canvas_history_snapshot_decref_nullable(m_data);
}

bool CanvasHistorySnapshot::isNull() const
{
	return !m_data;
}

int CanvasHistorySnapshot::historyOffset() const
{
	return DP_canvas_history_snapshot_history_offset(m_data);
}

int CanvasHistorySnapshot::historyCount() const
{
	return DP_canvas_history_snapshot_history_count(m_data);
}

const DP_CanvasHistoryEntry *
CanvasHistorySnapshot::historyEntryAt(int index) const
{
	return DP_canvas_history_snapshot_history_entry_at(m_data, index);
}

int CanvasHistorySnapshot::forkStart() const
{
	return DP_canvas_history_snapshot_fork_start(m_data);
}

int CanvasHistorySnapshot::forkFallbehind() const
{
	return DP_canvas_history_snapshot_fork_fallbehind(m_data);
}

int CanvasHistorySnapshot::forkCount() const
{
	return DP_canvas_history_snapshot_fork_count(m_data);
}

const DP_ForkEntry *CanvasHistorySnapshot::forkEntryAt(int index) const
{
	return DP_canvas_history_snapshot_fork_entry_at(m_data, index);
}

CanvasHistorySnapshot::CanvasHistorySnapshot(DP_CanvasHistorySnapshot *chs)
	: m_data{chs}
{
}

}
