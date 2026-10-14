//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

// iOS implementation of LogArchiver (common/android/log_archiver.h).
//
// An iOS application has no shared downloads folder. Its Documents folder is shown in the Files app
// ("On My iPhone" > the application), so the logs are copied into a folder there, from where they
// can be shared by mail or a messenger.

#include "common/android/log_archiver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
// static
LogArchiver::Result LogArchiver::saveToDownloads(QString* location)
{
    const QString dir_path = loggingDirectory();
    const QStringList names =
        QDir(dir_path).entryList(QStringList() << "*.log", QDir::Files, QDir::Name);

    if (names.isEmpty())
    {
        LOG(INFO) << "No log files in" << dir_path;
        return Result::NO_LOGS;
    }

    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
    {
        LOG(ERROR) << "No documents folder";
        return Result::FAILED;
    }

    const QString folder_name = QString("aspia-%1-logs-%2")
        .arg(QCoreApplication::applicationName().toLower(),
             QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    const QString target = documents + "/Logs/" + folder_name;

    if (!QDir().mkpath(target))
    {
        LOG(ERROR) << "Unable to create directory:" << target;
        return Result::FAILED;
    }

    for (const QString& file_name : names)
    {
        if (!QFile::copy(dir_path + "/" + file_name, target + "/" + file_name))
        {
            LOG(ERROR) << "Unable to copy log file:" << file_name;
            return Result::FAILED;
        }
    }

    if (location)
    {
        *location = QCoreApplication::translate("LogArchiver", "Files > On My iPhone > Aspia > Logs > %1")
            .arg(folder_name);
    }

    LOG(INFO) << "Logs are saved to" << target;
    return Result::SUCCESS;
}
