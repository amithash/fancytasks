/***********************************************************************************
* Fancy Tasks: Plasmoid for fancy representing your tasks and launchers.
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

#include "FancyTasksLight.h"
#include "FancyTasksApplet.h"
#include "FancyTasksIcon.h"
#include "FancyTasksTask.h"

#include <KMenu>
#include <KIconLoader>
#include <KIconEffect>

#include <Plasma/WindowEffects>
#include <Plasma/ToolTipManager>

namespace FancyTasks
{

Light::Light(WId window, Applet *applet, Icon *icon) : QGraphicsWidget(icon),
    m_applet(applet),
    m_icon(icon),
    m_task(NULL),
    m_window(window),
    m_dragTimer(0),
    m_highlightTimer(0)
{
    setAcceptsHoverEvents(true);

    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    setFlag(QGraphicsItem::ItemIsFocusable);

    Plasma::ToolTipManager::self()->registerWidget(this);

    TaskManager::TaskPtr task = TaskManager::TaskManager::self()->findTask(m_window);

    if (!task)
    {
        deleteLater();
    }

    m_task = new Task(new TaskManager::TaskItem(this, task), new TaskManager::GroupManager(this));
}

void Light::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QPixmap pixmap = m_applet->lightPixmap();

    if (m_color.isValid())
    {
        pixmap = KIconLoader::global()->iconEffect()->apply(pixmap, KIconEffect::Colorize, 1, m_color, true);
    }

    painter->setRenderHints(QPainter::SmoothPixmapTransform);
    painter->drawPixmap(boundingRect().toRect(), pixmap);
}

void Light::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    QList<WId> windows;
    windows.append(m_window);

    Plasma::ToolTipContent data;
    data.setMainText(m_task->title());
    data.setSubText(m_task->description());
    data.setImage(m_task->icon());
    data.setClickable(true);

    data.setWindowsToPreview(windows);

    Plasma::ToolTipManager::self()->setContent(this, data);

    m_highlightTimer = startTimer(500);

    setOpacity(0.7);
}

void Light::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    killTimer(m_highlightTimer);

    setOpacity(1);

    if (Plasma::WindowEffects::isEffectAvailable(Plasma::WindowEffects::HighlightWindows))
    {
        Plasma::WindowEffects::highlightWindows(m_applet->window(), QList<WId>());
    }
}

void Light::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    killTimer(m_dragTimer);

    m_dragTimer = startTimer(300);
}

void Light::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    killTimer(m_dragTimer);
}

void Light::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
}

void Light::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
       m_task->activate();

        event->accept();
    }
    else if (event->button() == Qt::MidButton)
    {
        m_task->close();

        event->accept();
    }
}

void Light::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    KMenu *menu = m_task->contextMenu();
    menu->addTitle(KIcon(m_task->icon()), m_task->title().left(20), menu->actions().at(0));
    menu->exec(QCursor::pos());

    delete menu;

    event->accept();
}

void Light::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_dragTimer && isUnderMouse())
    {
        m_task->activateWindow();
    }
    else if (event->timerId() == m_highlightTimer && Plasma::WindowEffects::isEffectAvailable(Plasma::WindowEffects::HighlightWindows))
    {
        Plasma::WindowEffects::highlightWindows(m_applet->window(), m_task->windows());
    }

    killTimer(event->timerId());
}

void Light::toolTipAboutToShow()
{
    connect(Plasma::ToolTipManager::self(), SIGNAL(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)), m_icon, SLOT(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)));
}

void Light::toolTipHidden()
{
    disconnect(Plasma::ToolTipManager::self(), SIGNAL(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)), m_icon, SLOT(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)));
}

void Light::setSize(qreal size)
{
    size *= 0.1;

    setPreferredSize(size, size);
}

void Light::setColor(QColor color)
{
    m_color = color;

    update();
}

}
