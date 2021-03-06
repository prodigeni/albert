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

#pragma once
#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QMutex>
#include <QFileSystemWatcher>
#include <vector>
using std::vector;
#include <memory>
#include "iextension.h"
#include "search/search.h"

namespace ChromeBookmarks {

class Bookmark;
class ConfigWidget;
class Indexer;

class Extension final : public QObject, public IExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ALBERT_EXTENSION_IID FILE "metadata.json")
    Q_INTERFACES(IExtension)

    friend class Indexer;

public:
    Extension();
    ~Extension();

    // GenericPluginInterface
    QWidget *widget(QWidget *parent = nullptr) override;

    // IExtension
    void handleQuery(shared_ptr<Query> query) override;

    const QString &path();
    void setPath(const QString &s);
    void restorePath();

    void updateIndex();

    bool fuzzy();
    void setFuzzy(bool b = true);

private:
    QPointer<ConfigWidget> _widget;
    vector<shared_ptr<Bookmark>> _index;
    Search _searchIndex;
    QMutex _indexAccess;
    QPointer<Indexer> _indexer;
    QString _bookmarksFile;
    QFileSystemWatcher _watcher;

    /* constexpr */
    static constexpr const char* EXT_NAME       = "chromebookmarks";
    static constexpr const char* CFG_BOOKMARKS  = "bookmarkfile";
    static constexpr const char* CFG_FUZZY      = "fuzzy";
    static constexpr const bool  CFG_FUZZY_DEF  = false;

signals:
    void pathChanged(const QString&);
    void statusInfo(const QString&);
};
}
