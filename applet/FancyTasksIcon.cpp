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

#define PI 3.141592653

#include "FancyTasksIcon.h"
#include "FancyTasksTask.h"
#include "FancyTasksLauncher.h"
#include "FancyTasksJob.h"
#include "FancyTasksLight.h"
#include "FancyTasksMenu.h"

#include <cmath>

#include <QMimeData>
#include <QTransform>
#include <QApplication>
#include <QGraphicsView>

#include <KMenu>
#include <KIcon>
#include <KLocale>
#include <KIconLoader>
#include <KIconEffect>
#include <NETRootInfo>
#include <KDesktopFile>
#include <KWindowSystem>

#include <KIO/NetAccess>
#include <KIO/PreviewJob>

#include <Plasma/Svg>
#include <Plasma/Theme>
#include <Plasma/Corona>
#include <Plasma/Animation>
#include <Plasma/WindowEffects>
#include <Plasma/ToolTipManager>

namespace FancyTasks
{

Icon::Icon(TaskManager::AbstractGroupableItem *abstractItem, Launcher *launcher, Job *job, Applet *parent) : QGraphicsWidget(parent),
    m_applet(parent),
    m_task(NULL),
    m_launcher(NULL),
    m_glowEffect(NULL),
    m_layout(new QGraphicsLinearLayout(this)),
    m_animationTimeLine(new QTimeLine(1000, this)),
    m_jobAnimationTimeLine(NULL),
    m_itemType(TypeOther),
    m_factor(parent->initialFactor()),
    m_animationProgress(-1),
    m_jobsProgress(0),
    m_jobsAnimationProgress(0),
    m_dragTimer(0),
    m_highlightTimer(0),
    m_menuVisible(false),
    m_demandsAttention(false),
    m_jobsRunning(false),
    m_isVisible(true),
    m_isPressed(false)
{
    setObjectName("FancyTasksIcon");

    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    setAcceptsHoverEvents(true);

    setAcceptDrops(true);

    setFocusPolicy(Qt::StrongFocus);

    setFlag(QGraphicsItem::ItemIsFocusable);

    setLayout(m_layout);

    m_visualizationPixmap = NULL;

    m_thumbnailPixmap = NULL;

    m_animationTimeLine->setFrameRange(0, 100);
    m_animationTimeLine->setUpdateInterval(50);
    m_animationTimeLine->setCurveShape(QTimeLine::LinearCurve);

    m_layout->setOrientation((m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::RightEdge)?Qt::Vertical:Qt::Horizontal);
    m_layout->addStretch();
    m_layout->addStretch();

    if (abstractItem)
    {
        setTask(abstractItem);
    }
    else if (launcher)
    {
        setLauncher(launcher);
    }
    else if (job)
    {
        addJob(job);
    }

    connect(this, SIGNAL(destroyed()), m_applet, SLOT(updateSize()));
    connect(this, SIGNAL(hoverMoved(QGraphicsWidget*, qreal)), m_applet, SLOT(itemHoverMoved(QGraphicsWidget*, qreal)));
    connect(this, SIGNAL(hoverLeft()), m_applet, SLOT(hoverLeft()));
    connect(m_applet, SIGNAL(sizeChanged(qreal)), this, SLOT(setSize(qreal)));
    connect(m_applet, SIGNAL(sizeChanged(qreal)), this, SIGNAL(sizeChanged(qreal)));
    connect(m_animationTimeLine, SIGNAL(finished()), this, SLOT(stopAnimation()));
    connect(m_animationTimeLine, SIGNAL(frameChanged(int)), this, SLOT(progressAnimation(int)));
}

void Icon::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_isVisible)
    {
        return;
    }

    qreal visualizationSize = (m_size * ((m_applet->moveAnimation() == ZoomAnimation)?m_factor:((m_applet->moveAnimation() == JumpAnimation)?m_applet->initialFactor():1)));

    if (m_isPressed)
    {
        visualizationSize *= 0.9;
    }

    if (visualizationSize < 1)
    {
        return;
    }

    QPixmap visualizationPixmap;
    QPixmap spotlightPixmap;
    qreal xOffset = 0;
    qreal yOffset = 0;
    qreal size = 0;
    qreal width = 0;
    qreal height = 0;

    switch (m_applet->location())
    {
        case Plasma::LeftEdge:
            xOffset = (m_size / 4);
        break;
        case Plasma::RightEdge:
            xOffset = (m_size - visualizationSize);
        break;
        case Plasma::TopEdge:
            yOffset = (m_size / 4);
        break;
        default:
            yOffset = (m_size - visualizationSize);
        break;
    }

    if (m_visualizationPixmap.isNull())
    {
        if (icon().isNull())
        {
            QTimer::singleShot(250, this, SLOT(updateIcon()));

            return;
        }

        QPixmap visualizationPixmap;
        visualizationPixmap = QPixmap(ceil(m_size), ceil(m_size));
        visualizationPixmap.fill(Qt::transparent);

        QPainter pixmapPainter(&visualizationPixmap);
        pixmapPainter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing | QPainter::TextAntialiasing);

        if (m_applet->useThumbnails() && !m_thumbnailPixmap.isNull() && m_itemType != TypeGroup)
        {
            QPixmap thumbnail = ((m_thumbnailPixmap.width() > m_thumbnailPixmap.height())?m_thumbnailPixmap.scaledToWidth(m_size, Qt::SmoothTransformation):m_thumbnailPixmap.scaledToHeight(m_size, Qt::SmoothTransformation));
            qreal iconSize = (m_size * 0.3);

            pixmapPainter.drawPixmap(((m_size - thumbnail.width()) / 2), ((m_size - thumbnail.height()) / 2), thumbnail);
            pixmapPainter.drawPixmap((m_size - iconSize), (m_size - iconSize), iconSize, iconSize, icon().pixmap(iconSize));
        }
        else
        {
            pixmapPainter.drawPixmap(0, 0, m_size, m_size, icon().pixmap(m_size));
        }

        pixmapPainter.end();

        m_visualizationPixmap = visualizationPixmap;
    }

    painter->setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing | QPainter::TextAntialiasing);

    QPixmap target = QPixmap(ceil(boundingRect().width()), ceil(boundingRect().height()));
    target.fill(Qt::transparent);

    QPainter targetPainter(&target);
    targetPainter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing | QPainter::TextAntialiasing);

    if (m_animationProgress >= 0)
    {
        visualizationPixmap = QPixmap(ceil(m_size), ceil(m_size));
        visualizationPixmap.fill(Qt::transparent);

        QPainter pixmapPainter(&visualizationPixmap);
        pixmapPainter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);

        switch (m_animationType)
        {
            case ZoomAnimation:
                size = ((m_size * 0.75) + (m_size * ((cos(2 * PI * m_animationProgress) + 0.5) / 8)));

                if (size > 1)
                {
                    pixmapPainter.drawPixmap(QRectF(((m_size - size) / 2), ((m_size - size) / 2), size, size), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));
                }
            break;
            case RotateAnimation:
                size = (m_size * (0.5 + (((m_animationProgress < 0.5)?m_animationProgress:(1 - m_animationProgress)) * 0.2)));

                if (size > 1)
                {
                    pixmapPainter.translate((m_size / 2), (m_size / 2));
                    pixmapPainter.rotate(360 * m_animationProgress);
                    pixmapPainter.drawPixmap(QRectF(-(size / 2), -(size / 2), size, size), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));
                }
            break;
            case BounceAnimation:
                width = (m_size * ((m_animationProgress < 0.5)?((m_animationProgress * 0.5) + 0.5):(1 - (m_animationProgress / 2))));
                height = (m_size * ((m_animationProgress < 0.5)?(1 - (m_animationProgress / 2)):((m_animationProgress * 0.5) + 0.5)));

                if (width > 1 && height > 1)
                {
                    pixmapPainter.drawPixmap(QRectF(((m_size - width) / 2), ((m_size - height) / 2), width, height), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));
                }
            break;
            case JumpAnimation:
                pixmapPainter.drawPixmap(QRectF(0, 0, m_size, m_size), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));

                if (m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::TopEdge)
                {
                    size = ((sin(2 * PI * m_animationProgress) + 1) / 2);
                }
                else
                {
                    size = ((cos(2 * PI * m_animationProgress) + 1) / 2);
                }

                switch (m_applet->location())
                {
                    case Plasma::LeftEdge:
                        xOffset += (size * (m_size / 4));
                    break;
                    case Plasma::RightEdge:
                        xOffset *= size;
                    break;
                    case Plasma::TopEdge:
                        yOffset += (size * (m_size / 4));
                    break;
                    default:
                        yOffset *= size;
                    break;
                }
            break;
            case BlinkAnimation:
                pixmapPainter.setOpacity(0.2 + ((cos(2 * PI * m_animationProgress) + 0.5) / 4));
                pixmapPainter.drawPixmap(QRectF(0, 0, m_size, m_size), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));
            break;
            case GlowAnimation:
                changeGlow(true, m_animationProgress);
            break;
            case SpotlightAnimation:
                spotlightPixmap = m_applet->theme()->pixmap("spotlight").scaled(m_size, m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                if (m_itemType == TypeGroup && m_task->color().isValid())
                {
                    spotlightPixmap = KIconLoader::global()->iconEffect()->apply(spotlightPixmap, KIconEffect::Colorize, 1, m_task->color(), true);
                }

                pixmapPainter.setOpacity((cos(2 * PI * m_animationProgress) + 1) / 4);
                pixmapPainter.drawPixmap(0, 0, spotlightPixmap);
                pixmapPainter.setOpacity(0.5);
                pixmapPainter.drawPixmap(0, 0, m_visualizationPixmap);
            break;
            default:
                pixmapPainter.drawPixmap(QRectF(0, 0, m_size, m_size), m_visualizationPixmap, QRectF(0, 0, m_size, m_size));
            break;
        }

        pixmapPainter.end();
    }
    else
    {
        visualizationPixmap = m_visualizationPixmap;
    }

    if (hasFocus())
    {
        targetPainter.drawPixmap(xOffset, yOffset, m_applet->theme()->pixmap("focus").scaled(visualizationSize, visualizationSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    if (m_task && m_task->isActive() && m_applet->activeIconIndication() == GlowIndication && m_applet->moveAnimation() != GlowAnimation)
    {
        changeGlow(true, 1);

        visualizationSize *= 0.8;
        xOffset += (visualizationSize * 0.15);
        yOffset += (visualizationSize * 0.15);
    }

    switch (m_applet->moveAnimation())
    {
        case ZoomAnimation:
            if (isUnderMouse() || hasFocus())
            {
                visualizationPixmap = KIconLoader::global()->iconEffect()->apply(visualizationPixmap, KIconLoader::Desktop, KIconLoader::ActiveState);
            }
        break;
        case GlowAnimation:
            changeGlow(true, m_factor);

            if (isUnderMouse() || hasFocus())
            {
                visualizationPixmap = KIconLoader::global()->iconEffect()->apply(visualizationPixmap, KIconLoader::Desktop, KIconLoader::ActiveState);
            }

            visualizationSize *= 0.8;
            xOffset += (visualizationSize * 0.15);
            yOffset += (visualizationSize * 0.15);
        break;
        case SpotlightAnimation:
            if (m_factor > 0)
            {
                spotlightPixmap = m_applet->theme()->pixmap("spotlight").scaled(visualizationSize, visualizationSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                targetPainter.setOpacity(m_factor);
                targetPainter.drawPixmap(0, 0, ((m_itemType == TypeGroup && m_task->color().isValid())?KIconLoader::global()->iconEffect()->apply(spotlightPixmap, KIconEffect::Colorize, 1, m_task->color().isValid(), true):spotlightPixmap));
                targetPainter.setOpacity(0.9);
            }
        break;
        case FadeAnimation:
            targetPainter.setOpacity(m_factor + 0.25);
        break;
        default:
        break;
    }

    if (m_task && m_task->isActive() && m_applet->activeIconIndication() == FadeIndication)
    {
        visualizationPixmap = KIconLoader::global()->iconEffect()->apply(visualizationPixmap, KIconLoader::Desktop, KIconLoader::ActiveState);
    }

    targetPainter.drawPixmap(QRectF(xOffset, yOffset, visualizationSize, visualizationSize), visualizationPixmap, visualizationPixmap.rect());

    if (m_jobsRunning && m_jobs.count())
    {
        qreal rotation = 0;

        QPixmap progressPixmap = m_applet->theme()->pixmap("progress").scaled((visualizationSize * 0.8), (visualizationSize * 0.8), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        targetPainter.save();
        targetPainter.translate(QPointF((xOffset + (visualizationSize * 0.5)), (yOffset + (visualizationSize * 0.5))));

        if (m_jobsProgress < 100)
        {
            rotation = (m_jobsProgress?(3.6 * m_jobsProgress):m_jobsAnimationProgress);

            targetPainter.rotate(rotation);
        }

        targetPainter.drawPixmap(QRectF(-(visualizationSize * 0.4), -(visualizationSize * 0.4), progressPixmap.width(), progressPixmap.width()), progressPixmap, progressPixmap.rect());

        targetPainter.restore();
    }

    if (m_applet->titleLabelMode() != NoLabel && !title().isEmpty() && (m_applet->titleLabelMode() == AlwaysShowLabel || (m_task && m_task->isActive() && m_applet->titleLabelMode() == LabelForActiveIcon) || (isUnderMouse() && m_applet->titleLabelMode() == LabelOnMouseOver)))
    {
        QFont font = targetPainter.font();
        font.setPixelSize(visualizationSize * 0.2);

        targetPainter.setFont(font);

        qreal textLength = (targetPainter.fontMetrics().width(title()) + (3 * targetPainter.fontMetrics().width(' ')));
        qreal textFieldWidth = ((textLength > (visualizationSize * 0.9))?(visualizationSize * 0.9):textLength);

        QRectF textField = QRectF(QPointF((((visualizationSize - textFieldWidth) / 2) + xOffset), ((target.height() * 0.55))), QSizeF(textFieldWidth, (visualizationSize * 0.25)));

        QPainterPath textFieldPath;
        textFieldPath.addRoundedRect(textField, 3, 3);

        targetPainter.setOpacity(0.6);
        targetPainter.fillPath(textFieldPath, QBrush(Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor)));
        targetPainter.setPen(QPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor).darker()));
        targetPainter.drawRoundedRect(textField, 3, 3);
        targetPainter.setPen(QPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor)));
        targetPainter.setOpacity(0.9);
        targetPainter.drawText(textField, ((textLength > textFieldWidth)?((QApplication::layoutDirection() == Qt::LeftToRight)?Qt::AlignLeft:Qt::AlignRight):Qt::AlignCenter), ((textLength > textFieldWidth)?(' ' + title()):title()));

        if (textLength > textFieldWidth)
        {
            QLinearGradient alphaGradient(0, 0, 1, 0);
            alphaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);

            if (QApplication::layoutDirection() == Qt::LeftToRight)
            {
                alphaGradient.setColorAt(0, QColor(0, 0, 0, 255));
                alphaGradient.setColorAt(0.8, QColor(0, 0, 0, 255));
                alphaGradient.setColorAt(1, QColor(0, 0, 0, 25));
            }
            else
            {
                alphaGradient.setColorAt(0, QColor(0, 0, 0, 25));
                alphaGradient.setColorAt(0.2, QColor(0, 0, 0, 255));
                alphaGradient.setColorAt(1, QColor(0, 0, 0, 255));
            }

            targetPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            targetPainter.fillPath(textFieldPath, alphaGradient);
            targetPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }

    if (m_applet->paintReflections())
    {
        QPointF reflectionPoint;
        QPixmap reflectionPixmap;
        QLinearGradient reflectionGradient;

        switch (m_applet->location())
        {
            case Plasma::LeftEdge:
                reflectionPoint = QPointF(0, 0);

                reflectionPixmap = target.copy((target.width() * 0.2), 0, (target.width() * 0.2), target.height());
                reflectionPixmap = reflectionPixmap.transformed(QTransform(-1, 0, 0, 0, 1, 0, 0, 0, 1), Qt::SmoothTransformation);

                reflectionGradient = QLinearGradient(QPointF(reflectionPixmap.width(), 0), QPointF(0, 0));
            break;
            case Plasma::RightEdge:
                reflectionPoint = QPointF((target.width() * 0.7), 0);

                reflectionPixmap = target.copy((target.width() * 0.5), 0, (target.width() * 0.2), target.height());
                reflectionPixmap = reflectionPixmap.transformed(QTransform(-1, 0, 0, 0, 1, 0, 0, 0, 1), Qt::SmoothTransformation);

                reflectionGradient = QLinearGradient(QPointF(0, 0), QPointF(reflectionPixmap.width(), 0));
            break;
            case Plasma::TopEdge:
                reflectionPoint = QPointF(0, 0);

                reflectionPixmap = target.copy(0, (m_size / 4), target.width(), (target.height() * 0.2));
                reflectionPixmap = reflectionPixmap.transformed(QTransform(1, 0, 0, 0, -1, 0, 0, 0, 1), Qt::SmoothTransformation);

                reflectionGradient = QLinearGradient(QPointF(0, reflectionPixmap.height()), QPointF(0, 0));
            break;
            default:
                reflectionPoint = QPointF(0, (target.height() * 0.7));

                reflectionPixmap = target.copy(0, (target.height() * 0.5), target.width(), (target.height() * 0.2));
                reflectionPixmap = reflectionPixmap.transformed(QTransform(1, 0, 0, 0, -1, 0, 0, 0, 1), Qt::SmoothTransformation);

                reflectionGradient = QLinearGradient(QPointF(0, 0), QPointF(0, reflectionPixmap.height()));
            break;
        }

        reflectionGradient.setColorAt(0, QColor(0, 0, 0, 200));
        reflectionGradient.setColorAt(0.6, QColor(0, 0, 0, 70));
        reflectionGradient.setColorAt(0.9, Qt::transparent);

        QPainter reflectionPainter(&reflectionPixmap);
        reflectionPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        reflectionPainter.fillRect(0, 0, reflectionPixmap.width(), reflectionPixmap.height(), reflectionGradient);
        reflectionPainter.end();

        targetPainter.drawPixmap(reflectionPoint, reflectionPixmap);
    }

    painter->drawPixmap(0, 0, target);
}

void Icon::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event)

    update();
}

void Icon::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)

    update();
}

void Icon::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    update();

    m_highlightTimer = startTimer(500);

    if (m_applet->moveAnimation() == JumpAnimation)
    {
        startAnimation(JumpAnimation, 500, false);
    }
}

void Icon::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    emit hoverMoved(this, (qreal) (((m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::RightEdge)?event->pos().y():event->pos().x()) / m_size));
}

void Icon::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    killTimer(m_highlightTimer);

    if (m_task && Plasma::WindowEffects::isEffectAvailable(Plasma::WindowEffects::HighlightWindows))
    {
        Plasma::WindowEffects::highlightWindows(m_applet->window(), QList<WId>());
    }

    m_isPressed = false;

    update();

    emit hoverLeft();
}

void Icon::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat("windowsystem/winid") || event->mimeData()->hasFormat("windowsystem/multiple-winids"))
    {
        event->acceptProposedAction();

        return;
    }

    killTimer(m_dragTimer);

    if (m_itemType == TypeTask || m_itemType == TypeGroup)
    {
        update();

        m_dragTimer = startTimer(300);
    }

    if (m_itemType != TypeLauncher)
    {
        event->ignore();
    }
}

void Icon::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    m_applet->itemDragged(this, event->pos(), event->mimeData());
}

void Icon::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    QTimer::singleShot(500, m_applet, SLOT(hideDropZone()));

    if (m_itemType != TypeLauncher)
    {
        event->ignore();
    }

    update();

    killTimer(m_dragTimer);
}

void Icon::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    m_applet->hideDropZone();

    if (m_applet->groupManager()->groupingStrategy() == TaskManager::GroupManager::ManualGrouping && (event->mimeData()->hasFormat("windowsystem/winid") || event->mimeData()->hasFormat("windowsystem/multiple-winids")) && (m_itemType == TypeTask || m_itemType == TypeGroup))
    {
        TaskManager::ItemList items;
        Icon *droppedIcon = m_applet->iconForMimeData(event->mimeData());

        if (droppedIcon)
        {
            if (event->mimeData()->hasFormat("windowsystem/winid"))
            {
                items.append(droppedIcon->task()->abstractItem());
            }
            else
            {
                items.append(droppedIcon->task()->group()->members());
            }

            m_task->dropItems(items);

            event->accept();

            return;
        }
    }
    else if (m_itemType == TypeLauncher && KUrl::List::canDecode(event->mimeData()))
    {
        m_launcher->dropUrls(KUrl::List::fromMimeData(event->mimeData()), event->modifiers());

        event->accept();

        return;
    }

    event->ignore();
}

void Icon::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Applet::setActiveWindow(KWindowSystem::activeWindow());

    m_isPressed = true;

    update();

    if (m_demandsAttention)
    {
        stopAnimation();

        m_demandsAttention = false;
    }

    if (m_itemType == TypeTask)
    {
        publishGeometry(m_task->task());
    }

    event->accept();
}

void Icon::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (QPoint(event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < QApplication::startDragDistance() || m_itemType == TypeStartup || (m_itemType == TypeLauncher && m_applet->immutability() != Plasma::Mutable))
    {
        return;
    }

    QMimeData *mimeData = new QMimeData;

    if (m_launcher)
    {
        m_launcher->launcherUrl().populateMimeData(mimeData);
    }

    if (m_itemType == TypeTask || m_itemType == TypeGroup)
    {
        m_task->abstractItem()->addMimeData(mimeData);
    }
    else if (m_itemType != TypeLauncher)
    {
        return;
    }

    QDrag *drag = new QDrag(event->widget());
    drag->setMimeData(mimeData);
    drag->setPixmap(m_visualizationPixmap.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    drag->exec();
}

void Icon::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isPressed)
    {
        m_isPressed = false;

        update();
    }

    QHash<IconAction, QPair<Qt::MouseButton, Qt::Modifier> > iconActions = m_applet->iconActions();
    QHash<IconAction, QPair<Qt::MouseButton, Qt::Modifier> >::iterator actionsIterator;

    for (actionsIterator = iconActions.begin(); actionsIterator != iconActions.end(); ++actionsIterator)
    {
        if (actionsIterator.value().first == Qt::NoButton)
        {
            continue;
        }

        if (actionsIterator.value().first == event->button() && (actionsIterator.value().second == Qt::UNICODE_ACCEL || event->modifiers() & actionsIterator.value().second))
        {
            performAction(actionsIterator.key());

            break;
        }
    }

    event->ignore();
}

void Icon::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        Plasma::ToolTipManager::self()->hide(this);
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        performAction(ActivateItem);
    }
    else
    {
        QGraphicsWidget::keyPressEvent(event);
    }
}

void Icon::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    performAction(ShowMenu);

    event->accept();
}

void Icon::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_dragTimer && isUnderMouse())
    {
        if (m_itemType == TypeTask)
        {
            m_task->activateWindow();
        }
        else if (m_itemType == TypeGroup)
        {
            performAction(ShowChildrenList);
        }
    }
    else if (event->timerId() == m_highlightTimer && Plasma::WindowEffects::isEffectAvailable(Plasma::WindowEffects::HighlightWindows) && (m_itemType == TypeTask || m_itemType == TypeGroup))
    {
        Plasma::WindowEffects::highlightWindows(m_applet->window(), m_task->windows());
    }

    killTimer(event->timerId());
}

void Icon::show()
{
    m_isVisible = true;

    if (m_itemType == TypeOther)
    {
        return;
    }

    updateSize();

    m_applet->updateSize();

    m_visualizationPixmap = NULL;

    update();
}

void Icon::hide()
{
    if (m_task)
    {
        return;
    }

    m_isVisible = false;

    updateSize();

    m_applet->updateSize();
}

void Icon::activate()
{
    QList<WId> windows;

    switch (m_itemType)
    {
        case TypeLauncher:
            performAction(ActivateLauncher);
        break;
        case TypeTask:
            performAction(ActivateTask);
        break;
        case TypeGroup:
            performAction(ShowChildrenList);
        break;
        default:
        break;
    }
}

void Icon::updateSize()
{
    if (!m_isVisible)
    {
        setPreferredSize(0, 0);

        return;
    }

    const qreal factor = ((m_applet->moveAnimation() == ZoomAnimation)?m_factor:((m_applet->moveAnimation() == JumpAnimation)?m_applet->initialFactor():1));
    const qreal width = (m_size * (factor + 0.1));
    const qreal height = (m_size * 1.4);

    if (m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::RightEdge)
    {
        setPreferredSize(height, width);
    }
    else
    {
        setPreferredSize(width, height);
    }
}

void Icon::updateIcon()
{
    update();
}

void Icon::setFactor(qreal factor)
{
    if (factor == m_factor)
    {
        return;
    }

    m_factor = factor;

    if (m_applet->moveAnimation() == ZoomAnimation)
    {
        if (m_applet->activeIconIndication() == ZoomIndication && m_task && m_task->isActive())
        {
            m_factor = 1;
        }

        updateSize();
    }

    if (!m_isVisible)
    {
        return;
    }

    update();
}

void Icon::setSize(qreal size)
{
    size *= 0.8;

    if (!m_isVisible)
    {
        m_size = size;
    }

    if (size == m_size)
    {
        return;
    }

    m_size = size;

    switch (m_applet->location())
    {
        case Plasma::LeftEdge:
            m_layout->setContentsMargins(0, 0, m_size, 0);
        break;
        case Plasma::RightEdge:
            m_layout->setContentsMargins(m_size, 0, 0, 0);
        break;
        case Plasma::TopEdge:
            m_layout->setContentsMargins(0, 0, 0, m_size);
        break;
        default:
            m_layout->setContentsMargins(0, m_size, 0, 0);
        break;
    }

    setThumbnail();

    updateSize();

    m_visualizationPixmap = NULL;

    update();
}

void Icon::setThumbnail(const KFileItem &item, const QPixmap thumbnail)
{
    Q_UNUSED(item)

    if (!m_applet->useThumbnails() || (m_itemType != TypeTask && (m_itemType != TypeLauncher || thumbnail.isNull())))
    {
        return;
    }

    if (m_itemType == TypeTask)
    {
        m_thumbnailPixmap = Applet::windowPreview(m_task->task()->task()->window(), ((200 > m_size)?200:m_size));
    }
    else
    {
        m_thumbnailPixmap = thumbnail;
    }

    m_visualizationPixmap = NULL;

    update();
}

void Icon::startAnimation(AnimationType animationType, int duration, bool repeat)
{
    m_animationType = animationType;

    m_animationTimeLine->setDuration(duration);
    m_animationTimeLine->setLoopCount(repeat?0:1);
    m_animationTimeLine->stop();
    m_animationTimeLine->start();
}

void Icon::stopAnimation()
{
    m_animationTimeLine->stop();

    setOpacity(1);

    m_animationProgress = -1;

    m_animationType = NoAnimation;

    update();
}

void Icon::progressAnimation(int progress)
{
    if (sender() == m_animationTimeLine)
    {
        m_animationProgress = ((qreal) progress / 100);
    }
    else if (sender() == m_jobAnimationTimeLine)
    {
        m_jobsAnimationProgress = progress;
    }

    update();
}

void Icon::changeGlow(bool enabled, qreal radius)
{
    if (enabled && !m_glowEffect)
    {
        m_glowEffect = new QGraphicsDropShadowEffect(this);
        m_glowEffect->setOffset(0);

        setGraphicsEffect(m_glowEffect);
    }

    m_glowEffect->setEnabled(enabled);
    m_glowEffect->setBlurRadius(radius * 25);
    m_glowEffect->setColor((m_itemType == TypeGroup && m_task->color().isValid())?m_task->color():Plasma::Theme::defaultTheme()->color(Plasma::Theme::HighlightColor));
}

void Icon::publishGeometry(TaskManager::TaskItem *task)
{
    if (!task || !task->task())
    {
        return;
    }

    QRect iconGeometry = shape().controlPointRect().toRect();

    if (scene() && iconGeometry.isValid())
    {
        QGraphicsView *parentView = NULL;
        QGraphicsView *possibleParentView = NULL;

        foreach (QGraphicsView *view, scene()->views())
        {
            if (view->sceneRect().intersects(sceneBoundingRect()) || view->sceneRect().contains(scenePos()))
            {
                if (view->isActiveWindow())
                {
                    parentView = view;

                    break;
                }
                else
                {
                    possibleParentView = view;
                }
            }
        }

        if (!parentView)
        {
            parentView = possibleParentView;
        }

        if (parentView)
        {
            iconGeometry = parentView->mapFromScene(mapToScene(iconGeometry)).boundingRect().adjusted(0, 0, 1, 1);
            iconGeometry.moveTopLeft(parentView->mapToGlobal(iconGeometry.topLeft()));
        }
    }

    task->task()->publishIconGeometry(iconGeometry);
}

void Icon::taskChanged(ItemChanges changes)
{
    if (!m_task)
    {
        return;
    }

    if (changes & OtherChanges)
    {
        if (m_task->taskType() == Task::TypeGroup)
        {
            emit colorChanged(m_task->color());
        }

        if (m_itemType == TypeStartup && m_task->taskType() != Task::TypeStartup)
        {
            stopAnimation();
        }

        if (m_task->taskType() == Task::TypeStartup)
        {
            m_itemType = TypeStartup;
        }
        else if (m_task->taskType() == Task::TypeTask)
        {
            m_itemType = TypeTask;

            setLauncher(m_applet->launcherForTask(m_task->task()));
        }
        else if (m_task->taskType() == Task::TypeGroup)
        {
            m_itemType = TypeGroup;
        }
    }

    if (changes & StateChanged)
    {
        if (m_task->abstractItem()->demandsAttention())
        {
            if (!m_demandsAttention)
            {
                m_applet->needsVisualFocus();
            }

            if (m_applet->demandsAttentionAnimation() != NoAnimation)
            {
                startAnimation(m_applet->demandsAttentionAnimation(), 1000, true);

                m_demandsAttention = true;
            }
        }
        else if (m_demandsAttention && !m_task->abstractItem()->demandsAttention())
        {
            stopAnimation();

            m_demandsAttention = false;
        }
    }

    if (changes & StateChanged && m_applet->activeIconIndication() == ZoomIndication && m_applet->moveAnimation() == ZoomAnimation)
    {
        setFactor(m_task->isActive()?1:m_applet->initialFactor());
    }

    if (changes & WindowsChanged && m_itemType == TypeTask)
    {
        QTimer::singleShot(200, this, SLOT(setThumbnail()));
    }

    if (changes & WindowsChanged || changes & TextChanged || changes & IconChanged)
    {
        updateToolTip();
    }

    if (changes & IconChanged)
    {
        m_visualizationPixmap = NULL;
    }

    update();
}

void Icon::launcherChanged(ItemChanges changes)
{
    if (!m_launcher || m_itemType != TypeLauncher)
    {
        return;
    }

    if (!m_launcher->isServiceGroup() && !KDesktopFile::isDesktopFile(m_launcher->targetUrl().toLocalFile()))
    {
        KFileItemList items;
        items.append(KFileItem(m_launcher->targetUrl(), m_launcher->mimeType()->name(), KFileItem::Unknown));

        QStringList plugins;
        qreal size = ((m_applet->itemSize() > 200)?m_applet->itemSize():200);

        KIO::PreviewJob *job = KIO::filePreview(items, size, size, 0, 0, true, true, &plugins);

        connect(job, SIGNAL(gotPreview(const KFileItem&, const QPixmap&)), this, SLOT(setThumbnail(const KFileItem&, const QPixmap&)));
    }

    if (changes & IconChanged)
    {
        m_visualizationPixmap = NULL;
    }

    update();

    updateToolTip();
}

void Icon::jobChanged(ItemChanges changes)
{
    Q_UNUSED(changes)

    int amount = 0;
    int percentage = 0;

    m_jobsRunning = false;

    if (m_jobs.count() > 0)
    {
        for (int i = 0; i < m_jobs.count(); ++i)
        {
            if (!m_jobs.at(i))
            {
                m_jobs.removeAt(i);

                --i;

                continue;
            }

            if (m_jobs.at(i)->state() != Job::Finished && !m_jobs.at(i)->state() != Job::Error)
            {
                ++amount;

                m_jobsRunning = true;

                percentage += ((m_jobs.at(i)->percentage() >= 0)?m_jobs.at(i)->percentage():0);
            }
        }
    }

    if (percentage)
    {
        percentage /= amount;
    }
    else if (m_jobsRunning)
    {
        if (!m_jobAnimationTimeLine)
        {
            m_jobAnimationTimeLine = new QTimeLine(5000, this);
            m_jobAnimationTimeLine->setLoopCount(0);
            m_jobAnimationTimeLine->setFrameRange(0, 359);
            m_jobAnimationTimeLine->setCurveShape(QTimeLine::LinearCurve);

            connect(m_jobAnimationTimeLine, SIGNAL(frameChanged(int)), this, SLOT(progressAnimation(int)));
        }

        if (m_jobAnimationTimeLine->state() != QTimeLine::Running)
        {
            m_jobAnimationTimeLine->start();
        }
    }

    if (!m_jobsRunning && m_jobAnimationTimeLine && m_jobAnimationTimeLine->state() == QTimeLine::Running)
    {
        m_jobAnimationTimeLine->stop();
        m_jobAnimationTimeLine->deleteLater();
        m_jobAnimationTimeLine = NULL;

        m_jobsAnimationProgress = 0;
    }

    m_jobsProgress = percentage;

    update();

    updateToolTip();
}

void Icon::jobDemandsAttention()
{
    if (!m_demandsAttention)
    {
        m_applet->needsVisualFocus();
    }

    if (m_applet->demandsAttentionAnimation() != NoAnimation)
    {
        startAnimation(m_applet->demandsAttentionAnimation(), 1000, true);

        m_demandsAttention = true;
    }

    jobChanged(StateChanged);
}

void Icon::setLauncher(Launcher *launcher)
{
    if (m_launcher && m_itemType != TypeLauncher)
    {
        m_launcher->removeItem(this);
    }

    m_launcher = launcher;

    if (m_launcher)
    {
        if (m_itemType == TypeOther)
        {
            m_itemType = TypeLauncher;
        }

        if (m_itemType != TypeLauncher)
        {
            m_launcher->addItem(this);
        }

        launcherChanged(EveythingChanged);

        if (m_itemType == TypeLauncher)
        {
            connect(m_launcher, SIGNAL(hide()), this, SLOT(hide()));
            connect(m_launcher, SIGNAL(show()), this, SLOT(show()));
        }
        else
        {
            disconnect(m_launcher, SIGNAL(hide()), this, SLOT(hide()));
            disconnect(m_launcher, SIGNAL(show()), this, SLOT(show()));
        }

        connect(m_launcher, SIGNAL(changed(ItemChanges)), this, SLOT(launcherChanged(ItemChanges)));
    }
}

void Icon::addJob(Job *job)
{
    if (m_jobs.indexOf(job) > -1)
    {
        return;
    }

    if (m_itemType == TypeOther)
    {
        m_itemType = TypeJob;
    }

    m_jobs.append(job);

    jobChanged(StateChanged);

    connect(job, SIGNAL(changed(ItemChanges)), this, SLOT(jobChanged(ItemChanges)));
    connect(job, SIGNAL(demandsAttention()), this, SLOT(jobDemandsAttention()));
    connect(job, SIGNAL(close(FancyTasksJob*)), this, SLOT(removeJob(FancyTasksJob*)));
}

void Icon::removeJob(Job *job)
{
    m_jobs.removeAll(job);

    if (m_itemType == TypeJob && !m_jobs.count())
    {
        deleteLater();

        return;
    }

    jobChanged(StateChanged);
}

void Icon::addWindow(WId window)
{
    if (m_windowLights.contains(window) || !KWindowSystem::hasWId(window) || m_windowLights.count() > 3)
    {
        return;
    }

    Light *taskLight = new Light(window, m_applet, this);
    taskLight->setSize(m_size);

    m_layout->insertItem(1, taskLight);
    m_layout->setAlignment(taskLight, (Qt::AlignBottom | Qt::AlignHCenter));

    m_windowLights[window] = taskLight;

    connect(this, SIGNAL(sizeChanged(qreal)), taskLight, SLOT(setSize(qreal)));
    connect(this, SIGNAL(colorChanged(QColor)), taskLight, SLOT(setColor(QColor)));
}

void Icon::removeWindow(WId window)
{
    if (m_windowLights.contains(window))
    {
        m_windowLights[window]->deleteLater();
        m_windowLights.remove(window);
    }

    if (m_task && m_task->windows().count() > m_windowLights.count())
    {
        QList<WId> windows = m_task->windows();

        for (int i = 0; i < windows.count(); ++i)
        {
            addWindow(windows.at(i));
        }
    }
}

void Icon::setTask(TaskManager::AbstractGroupableItem *abstractItem)
{
    if (abstractItem && !abstractItem->isGroupItem() && !static_cast<TaskManager::TaskItem*>(abstractItem)->startup().isNull())
    {
        if (m_applet->startupAnimation() != NoAnimation)
        {
            startAnimation(m_applet->startupAnimation(), 1000, true);

            QTimer::singleShot(30000, this, SLOT(stopAnimation()));
        }

        if (m_task)
        {
            return;
        }
    }

    if (!abstractItem)
    {
        if (m_task)
        {
            if (m_jobs.count())
            {
                m_itemType = TypeJob;
            }
            else if (m_launcher)
            {
                m_itemType = TypeLauncher;

                setLauncher(m_launcher);
            }
            else
            {
                m_itemType = TypeOther;
            }

            m_task->deleteLater();
            m_task = NULL;

            m_thumbnailPixmap = NULL;

            qDeleteAll(m_windowLights);

            m_windowLights.clear();

            updateToolTip();

            update();
        }

        return;
    }

    m_task = new Task(abstractItem, m_applet->groupManager());

    QList<WId> windowList = m_task->windows();

    for (int i = 0; i < windowList.count(); ++i)
    {
        addWindow(windowList.at(i));
    }

    taskChanged(EveythingChanged);

    connect(m_task, SIGNAL(changed(ItemChanges)), this, SLOT(taskChanged(ItemChanges)));
    connect(m_task, SIGNAL(publishGeometry(TaskManager::TaskItem*)), this, SLOT(publishGeometry(TaskManager::TaskItem*)));
    connect(m_task, SIGNAL(windowAdded(WId)), this, SLOT(addWindow(WId)));
    connect(m_task, SIGNAL(windowRemoved(WId)), this, SLOT(removeWindow(WId)));
}

void Icon::windowPreviewActivated(WId window, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, const QPoint &point)
{
    TaskManager::TaskPtr taskPointer = TaskManager::TaskManager::self()->findTask(window);

    Plasma::ToolTipManager::self()->hide(this);

    if (!taskPointer)
    {
        return;
    }

    TaskManager::GroupManager *groupManager = new TaskManager::GroupManager(this);
    Task *task = new Task(new TaskManager::TaskItem(groupManager, taskPointer), groupManager);

    if (buttons & Qt::LeftButton)
    {
        if (modifiers & Qt::ShiftModifier)
        {
            task->close();
        }
        else
        {
            task->activate();
        }
    }
    else if (buttons & Qt::RightButton)
    {
        KMenu *taskMenu = task->contextMenu();
        taskMenu->addTitle(task->icon(), task->title().left(20), taskMenu->actions().at(0));
        taskMenu->addActions(taskMenu->actions());
        taskMenu->exec(point);

        delete taskMenu;
    }

    delete task;
    delete groupManager;
}

void Icon::performAction(IconAction action)
{
    switch (action)
    {
        case ActivateItem:
            activate();
        break;
        case ActivateTask:
            if (m_task)
            {
                m_task->activate();
            }
        break;
        case ActivateLauncher:
            if (m_launcher)
            {
                if (m_launcher->isServiceGroup())
                {
                    m_menuVisible = true;

                    KMenu* menu = m_launcher->serviceMenu();
                    menu->exec(m_applet->containment()->corona()->popupPosition(this, menu->sizeHint(), Qt::AlignCenter));

                    delete menu;

                    m_menuVisible = false;
                }
                else
                {
                    m_launcher->activate();
                }
            }
        break;
        case ShowMenu:
            if (m_itemType == TypeLauncher || m_itemType == TypeJob || m_itemType == TypeTask || m_itemType == TypeGroup)
            {
                KMenu *menu;

                if (m_itemType == TypeTask || m_itemType == TypeGroup)
                {
                    menu = m_task->contextMenu();

                    if (m_launcher)
                    {
                        KMenu *launcherMenu = m_launcher->contextMenu();

                        menu->addSeparator();
                        menu->addMenu(launcherMenu);

                        connect(menu, SIGNAL(destroyed()), launcherMenu, SLOT(deleteLater()));
                    }
                }
                else if (m_itemType == TypeLauncher)
                {
                    menu = m_launcher->contextMenu();
                }
                else if (m_itemType == TypeJob && m_jobs.count() == 1)
                {
                    menu = m_jobs.at(0)->contextMenu();
                }
                else
                {
                    menu = new KMenu;
                }

                m_menuVisible = true;

                if (m_jobs.count() && (m_itemType != TypeJob || m_jobs.count() > 1))
                {
                    if (menu->actions().count())
                    {
                        menu->addSeparator();
                    }

                    for (int i = 0; i < m_jobs.count(); ++i)
                    {
                        QAction *action = menu->addAction(m_jobs.at(i)->icon(), m_jobs.at(i)->title());
                        action->setMenu(m_jobs.at(i)->contextMenu());
                    }
                }

                if (menu->actions().count())
                {
                    menu->addTitle(icon(), title().left(20), menu->actions().at(0));
                    menu->exec(m_applet->containment()->corona()->popupPosition(this, menu->sizeHint(), Qt::AlignCenter));
                }

                delete menu;

                m_menuVisible = false;
            }
        break;
        case ShowChildrenList:
            if (!m_menuVisible)
            {
                m_menuVisible = true;

                Applet::setActiveWindow(KWindowSystem::activeWindow());

                Menu *groupMenu = new Menu(m_task->windows());
                groupMenu->addSeparator();
                groupMenu->addAction(KIcon("process-stop"), i18nc("@action:inmenu", "Cancel"));
                groupMenu->exec(m_applet->containment()->corona()->popupPosition(this, groupMenu->sizeHint(), Qt::AlignCenter));

                delete groupMenu;

                m_menuVisible = false;
            }
        break;
        case ShowWindows:
            if (m_itemType == TypeGroup && Plasma::WindowEffects::isEffectAvailable(Plasma::WindowEffects::PresentWindowsGroup))
            {
                Plasma::WindowEffects::presentWindows(m_applet->window(), m_task->windows());
            }
        break;
        case CloseTask:
            if (m_itemType == TypeTask || m_itemType == TypeGroup)
            {
                m_task->close();
            }
        break;
        default:
        break;
    }
}

void Icon::toolTipAboutToShow()
{
    updateToolTip();

    connect(Plasma::ToolTipManager::self(), SIGNAL(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)), this, SLOT(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)));
}

void Icon::toolTipHidden()
{
    disconnect(Plasma::ToolTipManager::self(), SIGNAL(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)), this, SLOT(windowPreviewActivated(WId, Qt::MouseButtons, Qt::KeyboardModifiers, QPoint)));
}

void Icon::updateToolTip()
{
    if (m_itemType == TypeOther)
    {
        return;
    }

    QString progress;

    if (m_jobs.count())
    {
        if (m_jobsRunning && m_jobsProgress > 0)
        {
            progress.append(" <nobr><span style=\"color:" + Plasma::Theme::defaultTheme()->color(Plasma::Theme::HighlightColor).name() + ";\">");

            int bar = floor(m_jobsProgress * 0.2);

            for (int i = 0; i < 20; ++i)
            {
                progress.append("|");

                if (bar == i)
                {
                    progress.append("</span>");
                }
            }

            if (!progress.contains("</span>"))
            {
                progress.append("</span>");
            }

            progress.append(QChar(' ') + QString::number(m_jobsProgress) + "%</nobr>");
        }
    }

    Plasma::ToolTipContent data;
    data.setMainText(title() + progress);
    data.setSubText(description());
    data.setImage(icon());
    data.setClickable(true);

    if (m_itemType == TypeTask || m_itemType == TypeGroup)
    {
        data.setWindowsToPreview(m_task->windows());
    }

    Plasma::ToolTipManager::self()->setContent(this, data);
}

ItemType Icon::itemType() const
{
    return m_itemType;
}

QPointer<Task> Icon::task()
{
    return m_task;
}

QPointer<Launcher> Icon::launcher()
{
    return m_launcher;
}

QList<QPointer<Job> > Icon::jobs()
{
    return m_jobs;
}

QString Icon::title() const
{
    switch (m_itemType)
    {
        case TypeStartup:
        case TypeTask:
        case TypeGroup:
            return m_task->title();
        break;
        case TypeLauncher:
            return m_launcher->title();
        case TypeJob:
            if (m_jobs.count() > 1)
            {
                return i18np("1 job", "%1 jobs", m_jobs.count());
            }
            else if (m_jobs.count() == 1)
            {
                return m_jobs.at(0)->title();
            }
            else
            {
                return QString();
            }
        break;
        default:
            return QString();
        break;
    }
}

QString Icon::description() const
{
    QString description;

    switch (m_itemType)
    {
        case TypeStartup:
        case TypeTask:
        case TypeGroup:
            return m_task->description();
        break;
        case TypeLauncher:
            return m_launcher->description();
        case TypeJob:
            if (m_jobs.count() > 0)
            {
                for (int i = 0; i < m_jobs.count(); ++i)
                {
                    if (!m_jobs.at(i))
                    {
                        continue;
                    }

                    description.append(m_jobs.at(i)->information() + ((i < (m_jobs.count() - 1))?"<br />":QString()));
                }
            }

            if (m_jobs.count() == 1)
            {
                description = m_jobs.at(0)->description();
            }

            return description;
        break;
        default:
            return QString();
        break;
    }
}

QPainterPath Icon::shape() const
{
    QRectF rectangle;
    const qreal size = (m_size * ((m_applet->moveAnimation() == ZoomAnimation)?m_factor:((m_applet->moveAnimation() == JumpAnimation)?m_applet->initialFactor():1)));

    switch (m_applet->location())
    {
        case Plasma::LeftEdge:
            rectangle = QRectF((boundingRect().width() - m_size), 0, size, boundingRect().height());
        break;
        case Plasma::RightEdge:
            rectangle = QRectF((m_size - size), 0, size, boundingRect().height());
        break;
        case Plasma::TopEdge:
            rectangle = QRectF(0, (boundingRect().height() - m_size), boundingRect().width(), size);
        break;
        default:
            rectangle = QRectF(0, (m_size - size), boundingRect().width(), size);
        break;
    }

    QPainterPath path;
    path.addRect(rectangle);

    return path;
}

KIcon Icon::icon()
{
    switch (m_itemType)
    {
        case TypeStartup:
        case TypeTask:
        case TypeGroup:
            return (m_task?m_task->icon():KIcon());
        break;
        case TypeLauncher:
            return (m_launcher?m_launcher->icon():KIcon());
        case TypeJob:
            return m_jobs.at(0)->icon();
        break;
        default:
            return KIcon();
        break;
    }
}

qreal Icon::factor() const
{
    return m_factor;
}

bool Icon::isVisible() const
{
    return m_isVisible;
}

bool Icon::demandsAttention() const
{
    return m_demandsAttention;
}

}
