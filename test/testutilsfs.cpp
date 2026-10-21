/*
 * Bittorrent Client using Qt and libtorrent.
<<<<<<<< HEAD:test/testutilsmisc.cpp
 * Copyright (C) 2026  Mark Yu (vafada)
========
 * Copyright (C) 2026  Andy Ye
>>>>>>>> 05fcaa8f505f0657d5cd7b7c377282b63855e8ae:test/testutilsfs.cpp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include <QObject>
#include <QTest>

<<<<<<<< HEAD:test/testutilsmisc.cpp
#include "base/utils/misc.h"

class TestUtilsMisc final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsMisc)

public:
    TestUtilsMisc() = default;

private slots:
    void testfriendlyUnitCompact() const
    {
        QCOMPARE(Utils::Misc::friendlyUnitCompact(500), (u"500" + QChar::Nbsp + u"B"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000), (u"0.97" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(5000), (u"4.88" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000), (u"9.76" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000), (u"14.6" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(100000), (u"97.6" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(150000), (u"146" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000000), (u"976" + QChar::Nbsp + u"K"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000), (u"9.53" + QChar::Nbsp + u"M"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000000), (u"14.3" + QChar::Nbsp + u"M"));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000000), (u"9.31" + QChar::Nbsp + u"G"));
    }
};

QTEST_APPLESS_MAIN(TestUtilsMisc)
#include "testutilsmisc.moc"
========
#include "base/global.h"
#include "base/utils/fs.h"

class TestUtilsFs final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsFs)

public:
    TestUtilsFs() = default;

private slots:
    void testToValidFileName() const
    {
        QCOMPARE(Utils::Fs::toValidFileName(u"///"_s), u"_"_s);
        QCOMPARE(Utils::Fs::toValidFileName(u"///"_s, {}), QString());
        QCOMPARE(Utils::Fs::toValidFileName(u"."_s, {}), QString());
        QCOMPARE(Utils::Fs::toValidFileName(u" a/b "_s, {}), u"ab"_s);
    }
};

QTEST_APPLESS_MAIN(TestUtilsFs)
#include "testutilsfs.moc"
>>>>>>>> 05fcaa8f505f0657d5cd7b7c377282b63855e8ae:test/testutilsfs.cpp
