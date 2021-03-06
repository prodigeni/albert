// albert - a simple application launcher for linux
// Copyright (C) 2014-2015 Manuel Schneider
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QStandardPaths>
#include <QDirIterator>
#include <QThreadPool>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QDir>
#include "configwidget.h"
#include "extension.h"
#include "indexer.h"
#include "bookmark.h"
#include "query.h"



/** ***************************************************************************/
ChromeBookmarks::Extension::Extension() {
    qDebug() << "[ChromeBookmarks] Initialize extension";

    // Load settings
    QSettings s;
    s.beginGroup(EXT_NAME);
    _searchIndex.setFuzzy(s.value(CFG_FUZZY, CFG_FUZZY_DEF).toBool());

    // Deserialize data
    QFile dataFile(
                QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).
                filePath(QString("%1.dat").arg(EXT_NAME))
                );
    if (dataFile.exists())
        if (dataFile.open(QIODevice::ReadOnly| QIODevice::Text)) {
            qDebug() << "[ChromeBookmarks] Deserializing from" << dataFile.fileName();
            QDataStream in(&dataFile);
            quint64 size;
            QString name, url;
            short usage;
            in >> size;
            for (quint64 i = 0; i < size; ++i) {
                in >> name >> url >> usage;
                _index.push_back(std::make_shared<Bookmark>(name, url , usage));
            }
            dataFile.close();
        } else
            qWarning() << "Could not open file: " << dataFile.fileName();

    // Load and set a valid path (Updates the bookmarks)
    QVariant v = s.value(CFG_BOOKMARKS);
    if (v.isValid() && v.canConvert(QMetaType::QString) && QFileInfo(v.toString()).exists())
        setPath(v.toString());
    else
        restorePath();

    // Keep in sync with the bookmarkfile
    connect(&_watcher, &QFileSystemWatcher::fileChanged, this, &Extension::updateIndex);

    // Get a generic favicon
    Bookmark::icon_ = QIcon::fromTheme("favorites", QIcon(":favicon"));

    qDebug() << "[ChromeBookmarks] Extension initialized";
}



/** ***************************************************************************/
ChromeBookmarks::Extension::~Extension() {
    qDebug() << "[ChromeBookmarks] Finalize extension";

    // Stop and wait for background indexer
    if (!_indexer.isNull()) {
        _indexer->abort();
        disconnect(_indexer.data(), &Indexer::destroyed, this, &Extension::updateIndex);
        QEventLoop loop;
        connect(_indexer.data(), &Indexer::destroyed, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // Save settings
    QSettings s;
    s.beginGroup(EXT_NAME);
    s.setValue(CFG_FUZZY, _searchIndex.fuzzy());
    s.setValue(CFG_BOOKMARKS, _bookmarksFile);

    // Serialize data
    QFile dataFile(
                QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).
                filePath(QString("%1.dat").arg(EXT_NAME))
                );
    if (dataFile.open(QIODevice::ReadWrite| QIODevice::Text)) {
        qDebug() << "[ChromeBookmarks] Serializing to" << dataFile.fileName();
        QDataStream out( &dataFile );

        // Lock index against indexer
        QMutexLocker locker(&_indexAccess);

        // Serialize
        out << static_cast<quint64>(_index.size());
        for (shared_ptr<Bookmark> b : _index)
            out << b->name_ << b->url_ << b->usage_;

        dataFile.close();
    } else
        qCritical() << "Could not write to " << dataFile.fileName();

    qDebug() << "[ChromeBookmarks] Extension finalized";
}



/** ***************************************************************************/
QWidget *ChromeBookmarks::Extension::widget(QWidget *parent) {
    if (_widget.isNull()){
        _widget = new ConfigWidget(parent);

        // Paths
        _widget->ui.lineEdit_path->setText(_bookmarksFile);
        connect(_widget.data(), &ConfigWidget::requestEditPath, this, &Extension::setPath);
        connect(this, &Extension::pathChanged, _widget->ui.lineEdit_path, &QLineEdit::setText);

        // Fuzzy
        _widget->ui.checkBox_fuzzy->setChecked(fuzzy());
        connect(_widget->ui.checkBox_fuzzy, &QCheckBox::toggled, this, &Extension::setFuzzy);

        // Info
        _widget->ui.label_info->setText(QString("%1 bookmarks indexed.").arg(_index.size()));
        connect(this, &Extension::statusInfo, _widget->ui.label_info, &QLabel::setText);

        // If indexer is active connect its statusInfo to the infoLabel
        if (!_indexer.isNull())
            connect(_indexer.data(), &Indexer::statusInfo, _widget->ui.label_info, &QLabel::setText);
    }
    return _widget;
}



/** ***************************************************************************/
void ChromeBookmarks::Extension::handleQuery(shared_ptr<Query> query) {
    // Search for matches. Lock memory against indexer
    _indexAccess.lock();
    vector<shared_ptr<IIndexable>> indexables = _searchIndex.search(query->searchTerm());
    _indexAccess.unlock();

    // Add results to query. This cast is safe since index holds files only
    for (shared_ptr<IIndexable> obj : indexables)
        query->addMatch(std::static_pointer_cast<Bookmark>(obj),
                        std::static_pointer_cast<Bookmark>(obj)->usage());
}



/** ***************************************************************************/
const QString &ChromeBookmarks::Extension::path() {
    return _bookmarksFile;
}



/** ***************************************************************************/
void ChromeBookmarks::Extension::setPath(const QString &s) {
    QFileInfo fi(s);
    // Only let _existing_ _files_ in
    if (!(fi.exists() && fi.isFile()))
        return;

    if(!_watcher.addPath(s)) // No clue why this should happen
        qCritical() << s <<  "could not be watched. Changes in this path will not be noticed.";

    _bookmarksFile = s;
    updateIndex();

    // And update the widget, if it is visible atm
    emit pathChanged(s);
}



/** ***************************************************************************/
void ChromeBookmarks::Extension::restorePath() {
    // Find a bookmark file (Take first one)
    for (QString browser : {"chromium","google-chrome"}){
        QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath(browser);
        QDirIterator it(root, {"Bookmarks"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            setPath(it.next());
            return;
        }
    }
}



/** ***************************************************************************/
void ChromeBookmarks::Extension::updateIndex() {
    qDebug() << "[ChromeBookmarks] Index update triggered";

    // If thread is running, stop it and start this functoin after termination
    if (!_indexer.isNull()) {
        _indexer->abort();
        _widget->ui.label_info->setText("Waiting for indexer to shut down ...");
        connect(_indexer.data(), &Indexer::destroyed, this, &Extension::updateIndex);
    } else {
        // Create a new scanning runnable for the threadpool
        _indexer = new Indexer(this);

        //  Run it
        QThreadPool::globalInstance()->start(_indexer);

        // If widget is visible show the information in the status bat
        if (!_widget.isNull())
            connect(_indexer.data(), &Indexer::statusInfo, _widget->ui.label_info, &QLabel::setText);
    }
}



/** ***************************************************************************/
bool ChromeBookmarks::Extension::fuzzy() {
    return _searchIndex.fuzzy();
}



/** ***************************************************************************/
void ChromeBookmarks::Extension::setFuzzy(bool b) {
    _indexAccess.lock();
    _searchIndex.setFuzzy(b);
    _indexAccess.unlock();
}

