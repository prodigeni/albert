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

#include <QDebug>
#include "extension.h"
#include "query.h"
#include "predefinedobjects.h"
#include "abstractobjects.h"



/** ***************************************************************************/
Calculator::Extension::Extension() {
    qDebug() << "[Calculator] Initialize extension";
    if (QIcon::hasThemeIcon("calc"))
        calcIcon_ = QIcon::fromTheme("calc");
    else
        calcIcon_ = QIcon::fromTheme("unknown");  // FIXME FAVICON RESOURCE
    parser_.reset(new mu::Parser);
    parser_->SetDecSep(loc.decimalPoint().toLatin1());
    parser_->SetThousandsSep(loc.groupSeparator().toLatin1());
    qDebug() << "[Calculator] Extension initialized";
}



/** ***************************************************************************/
Calculator::Extension::~Extension() {
    parser_.reset();
}



/** ***************************************************************************/
void Calculator::Extension::handleQuery(shared_ptr<Query> query) {
    std::shared_ptr<StandardItem> calcItem = std::make_shared<StandardItem>();
    parser_->SetExpr(query->searchTerm().toStdString());
    QString result;
    try {
        result = loc.toString(parser_->Eval());
    }
    catch (mu::Parser::exception_type &e) {
      return;
    }
    calcItem->setName(result);
    calcItem->setInfo(QString("Result of '%1'").arg(query->searchTerm()));
    calcItem->setIcon(calcIcon_);
    calcItem->setAction(std::unique_ptr<CopyToClipboardAction>(
                            new CopyToClipboardAction(result)
                            ));
    query->addMatch(calcItem, SHRT_MAX);
}
