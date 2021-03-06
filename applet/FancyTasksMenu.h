/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
* Copyright (C) 2009-2010 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
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
***********************************************************************************/

#ifndef FANCYTASKSMENU_HEADER
#define FANCYTASKSMENU_HEADER

#include "FancyTasksApplet.h"

#include <KMenu>

namespace FancyTasks
{

class Menu : public KMenu
{
    Q_OBJECT

    public:
        Menu(QList<WId> windows = QList<WId>(), QWidget *parent = NULL);

        QAction* addAction(WId window);
        QAction* addAction(const QIcon &icon, const QString &text, WId window = 0);

    protected:
        void dragEnterEvent(QDragEnterEvent *event);
        void dragMoveEvent(QDragMoveEvent *event);
        void dragLeaveEvent(QDragLeaveEvent *event);
        void dropEvent(QDropEvent *event);
        void mousePressEvent(QMouseEvent *event);
        void mouseMoveEvent(QMouseEvent *event);
        void mouseReleaseEvent(QMouseEvent *event);
        void contextMenuEvent(QContextMenuEvent *event);

    private:
        QPoint m_dragStartPosition;
        QAction *m_currentAction;
};

}

#endif
