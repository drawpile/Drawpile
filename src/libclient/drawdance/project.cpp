// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/drawdance/project.h"
#include <QString>

namespace drawdance {

Project::~Project()
{
	if(m_data) {
		closeWarn();
	}
}

bool Project::open(
	const QString &path, unsigned int flags, int *outError, int *outSqlResult)
{
	DP_ProjectOpenResult result =
		DP_project_open(path.toUtf8().constData(), flags);
	if(outError) {
		*outError = result.error;
	}
	if(outSqlResult) {
		*outSqlResult = result.sql_result;
	}

	if(result.error == 0) {
		if(m_data) {
			closeWarn();
		}
		m_data = result.project;
		return true;
	} else {
		return false;
	}
}

bool Project::close()
{
	bool ok = DP_project_close(m_data);
	m_data = nullptr;
	return ok;
}

int Project::info(unsigned int flags, InfoFn fn)
{
	return DP_project_info(m_data, flags, &Project::infoCallback, &fn);
}

DP_ProjectCheckResult Project::checkPath(const QString &path)
{
	return DP_project_check_path(path.toUtf8().constData());
}

void Project::closeWarn()
{
	if(!DP_project_close(m_data)) {
		qWarning("Error closing project: %s", DP_error());
	}
}

void Project::infoCallback(void *user, const DP_ProjectInfo *info)
{
	(*static_cast<InfoFn *>(user))(*info);
}

}
