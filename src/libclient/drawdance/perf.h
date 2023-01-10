#ifndef DRAWDANCE_PERF_H
#define DRAWDANCE_PERF_H

extern "C" {
#define DP_PERF_NO_BEGIN_END
#include <dpcommon/perf.h>
}

#include <QString>

// We want to include the line number in the variable name for our RAII wrapper.
// That kind of macro-expanded token pasting requires preprocessor juggling.
#define DP_PERF_PASTE(A, B) A##B
#define DP_PERF_XPASTE(A, B) DP_PERF_PASTE(A, B)

#define DP_PERF_SCOPE(CATEGORIES) \
	drawdance::PerfScope DP_PERF_XPASTE(_perf_scope_, __LINE__){ \
		DP_perf_begin(DP_PERF_XSTR(DP_PERF_REALM), DP_PERF_CONTEXT ":" CATEGORIES, NULL)}

#define DP_PERF_SCOPE_DETAIL(CATEGORIES, ...) \
	drawdance::PerfScope DP_PERF_XPASTE(_perf_scope_, __LINE__) { \
		DP_perf_begin(DP_PERF_XSTR(DP_PERF_REALM), DP_PERF_CONTEXT ":" CATEGORIES, __VA_ARGS__)}

namespace drawdance {

class Perf final {
public:
	static bool open(const QString &path);
	static bool close();
	static bool isOpen();

private:
	static Perf instance;
	~Perf();
};

class PerfScope final {
public:
	explicit PerfScope(int handle) : m_handle{handle} {}
	~PerfScope() { DP_perf_end(m_handle); }

private:
	int m_handle;
};

} // namespace drawdance

#endif
