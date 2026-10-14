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

#include "base/sys_info.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"

#import <UIKit/UIKit.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

namespace {

//--------------------------------------------------------------------------------------------------
QString toQString(NSString* string)
{
    if (!string)
        return QString();

    return QString::fromNSString(string);
}

//--------------------------------------------------------------------------------------------------
// Peers accept only letters, digits, spaces and ".-_+()" in a computer name (see
// isValidComputerNameChar() in base/peer/authenticator.cc). The machine identifier has a comma
// ("iPhone17,1") and the name the user gave the device may have an apostrophe, so everything else
// is turned into a separator.
QString sanitizeComputerName(const QString& name)
{
    QString result;
    result.reserve(name.size());

    for (const QChar& ch : name)
    {
        if (ch.isLetterOrNumber() || QStringView(u".-_+() ").contains(ch))
            result.append(ch);
        else if (ch == u',')
            result.append(u'.');
        else
            result.append(u' ');
    }

    return result.simplified();
}

//--------------------------------------------------------------------------------------------------
// The machine identifier of the device ("iPhone15,2" and so on).
QString machineName()
{
    struct utsname info;
    if (uname(&info) < 0)
        return QString();

    return QString::fromLocal8Bit(info.machine);
}

} // namespace

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    @autoreleasepool
    {
        QString name = toQString([[UIDevice currentDevice] systemName]);
        if (name.isEmpty())
            name = "iOS";

        const QString version = operatingSystemVersion();
        if (version.isEmpty())
            return name;

        return name + " " + version;
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    @autoreleasepool
    {
        return toQString([[UIDevice currentDevice] systemVersion]);
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemDir()
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
quint64 SysInfo::uptime()
{
    struct timeval boot_time;
    size_t size = sizeof(boot_time);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };

    if (sysctl(mib, 2, &boot_time, &size, nullptr, 0) != 0 || boot_time.tv_sec == 0)
        return 0;

    const time_t now = time(nullptr);
    if (now < boot_time.tv_sec)
        return 0;

    return static_cast<quint64>(now - boot_time.tv_sec);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    // Since iOS 16 the name of the device is given only with a special entitlement, without it the
    // generic model name ("iPhone") comes back. The machine identifier tells more than that.
    @autoreleasepool
    {
        QString name = toQString([[UIDevice currentDevice] name]);
        const QString model = toQString([[UIDevice currentDevice] model]);

        name = sanitizeComputerName(name);
        if (!name.isEmpty() && name != model)
            return name;
    }

    const QString machine = sanitizeComputerName(machineName());
    if (!machine.isEmpty())
        return machine;

    return "iOS device";
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerDomain()
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorPackages()
{
    // Mobile devices have a single SoC.
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    int cores = 0;
    size_t size = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) != 0)
        return 0;

    return cores;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorThreads()
{
    long res = sysconf(_SC_NPROCESSORS_CONF);
    if (res == -1)
        return 1;

    return static_cast<int>(res);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray SysInfo::smbiosDump()
{
    NOTIMPLEMENTED();
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray SysInfo::hardwareId()
{
    // iOS exposes no hardware serial to applications. The identifier for the vendor is stable for
    // as long as at least one application of the vendor stays installed on the device.
    @autoreleasepool
    {
        NSUUID* uuid = [[UIDevice currentDevice] identifierForVendor];
        if (!uuid)
            return QByteArray();

        const QByteArray id = toQString([uuid UUIDString]).toUtf8();
        if (id.isEmpty())
            return QByteArray();

        // The source data is hashed to hide its format and to fix the length of the identifier.
        return GenericHash::hash(GenericHash::SHA256, id);
    }
}
