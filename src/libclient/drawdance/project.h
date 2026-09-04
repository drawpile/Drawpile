// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_PROJECT_H
#define LIBCLIENT_DRAWDANCE_PROJECT_H
extern "C" {
#include <dpengine/project.h>
}
#include <functional>

class QString;

namespace drawdance {

class Project final {
public:
	using InfoFn = std::function<void(const DP_ProjectInfo &)>;

	Project() = default;
	~Project();

	Project(const Project &) = delete;
	Project(Project &&) = delete;
	Project &operator=(const Project &) = delete;
	Project &operator=(Project &&) = delete;

	DP_Project *get() { return m_data; }

	bool open(
		const QString &path, unsigned int flags, int *outError = nullptr,
		int *outSqlResult = nullptr);

	bool close();

	int info(unsigned int flags, InfoFn fn);

	static DP_ProjectCheckResult checkPath(const QString &path);

private:
	void closeWarn();

	static void infoCallback(void *user, const DP_ProjectInfo *info);

	DP_Project *m_data = nullptr;
};

}

#endif
