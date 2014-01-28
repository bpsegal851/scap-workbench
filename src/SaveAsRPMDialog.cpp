/*
 * Copyright 2014 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Martin Preisler <mpreisle@redhat.com>
 */

#include "SaveAsRPMDialog.h"
#include "TemporaryDir.h"
#include "ScanningSession.h"
#include "ProcessHelpers.h"

#include <QFileDialog>
#include <cassert>

SaveAsRPMDialog::SaveAsRPMDialog(ScanningSession* session, QWidget* parent):
    QDialog(parent),

    mScanningSession(session)
{
    mUI.setupUi(this);

    QObject::connect(
        this, SIGNAL(finished(int)),
        this, SLOT(slotFinished(int))
    );

    // See https://fedoraproject.org/wiki/Packaging:NamingGuidelines#CommonCharacterSet
    // Furthermore, we do not allow '.' to avoid confusion.
    mUI.packageName->setValidator(new QRegExpValidator(QRegExp("^[a-zA-Z0-9\\-_\\+]+$"), this));

    const QFileInfo openedFile(mScanningSession->getOpenedFilePath());
    mUI.packageName->setText(openedFile.baseName());

    mUI.version->setValidator(new QRegExpValidator(QRegExp("^([0-9]+\\.)*[0-9]+$"), this));

    show();
}

SaveAsRPMDialog::~SaveAsRPMDialog()
{}

void SaveAsRPMDialog::slotFinished(int result)
{
    if (result == QDialog::Rejected)
        return;

    const QString targetDir = QFileDialog::getExistingDirectory(this, "Select target directory");
    if (targetDir.isEmpty())
        return; // user canceled

    QSet<QString> closure = mScanningSession->getOpenedFilesClosure();
    // At this point, closure is a set which is implementation ordered.
    // (we have no control WRT the ordering)
    // We want to make the XCCDF/SDS/main file appear first because that's
    // what the 'save as RPM' script will use to deduce the package name
    closure.remove(mScanningSession->getOpenedFilePath());
    QList<QString> closureOrdered;
    closureOrdered.append(mScanningSession->getOpenedFilePath());
    closureOrdered.append(closure.toList());

    const QDir cwd = ScanningSession::getCommonAncestorDirectory(closure);

    SyncProcess scapAsRPM(this);
    scapAsRPM.setCommand(SCAP_WORKBENCH_LOCAL_SCAP_AS_RPM_PATH);
    scapAsRPM.setWorkingDirectory(cwd.absolutePath());

    QStringList args;
    args.append("--pkg-name"); args.append(mUI.packageName->text());
    args.append("--pkg-version"); args.append(mUI.version->text());
    args.append("--pkg-release"); args.append(mUI.release->text());
    args.append("--rpm-destination"); args.append(targetDir);

    for (QList<QString>::const_iterator it = closureOrdered.begin(); it != closureOrdered.end(); ++it)
    {
        args.append(cwd.relativeFilePath(*it));
    }

    TemporaryDir tailoringDir;

    // Tailoring file is a special case since it may be in memory only.
    // In case it is memory only we don't want it to cause our common ancestor dir to be /
    // We export it to a temporary directory and remove it after including it in the RPM
    if (mScanningSession->hasTailoring())
    {
        QFileInfo tailoringFile(mScanningSession->getTailoringFilePath());
        assert(tailoringFile.exists());

        const QString tailoringFilePath = QString("%1/%2").arg(tailoringDir.getPath(), "tailoring-xccdf.xml");

        ScanningSession::copyOrReplace(tailoringFile.absoluteFilePath(),
            tailoringFilePath);

        args.append(tailoringFilePath);
    }

    scapAsRPM.setArguments(args);

    scapAsRPM.runWithDialog(this, "Saving SCAP content as RPM...", true, false);
}