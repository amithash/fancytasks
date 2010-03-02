/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
* Copyright (C) 2009-2010 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
*   Copyright 2007 by Alex Merry <alex.merry@kdemail.net>
*   Copyright 2008 by Alexis Ménard <darktears31@gmail.com>
*   Copyright 2010 by Amithash Prasad <amithash@gmail.com>
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

#include "FancyPanel.h"

#include <limits>
#include <cmath>

#include <QAction>
#include <QApplication>
#include <QBitmap>
#include <QComboBox>
#include <QDesktopWidget>
#include <QGraphicsLinearLayout>
#include <QGridLayout>
#include <QGraphicsLayout>
#include <QGraphicsSceneDragDropEvent>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QPainter>
#include <QSignalMapper>
#include <QTransform>
#include <QTimeLine>


#include <KDebug>
#include <KIcon>
#include <KMenu>
#include <KDialog>
#include <KConfigGroup>
#include <KIntNumInput>
#include <KMessageBox>

#include <Plasma/Corona>
#include <Plasma/FrameSvg>
#include <Plasma/Theme>
#include <Plasma/AbstractToolBox>
#include <Plasma/View>
#include <Plasma/PaintUtils>

#include <kephal/screens.h>

using namespace Plasma;

class Spacer : public QGraphicsWidget
{
public:
    Spacer(QGraphicsWidget *parent)
         : QGraphicsWidget(parent),
           m_visible(true)
    {
        setAcceptDrops(true);
    }

    ~Spacer()
    {}

    FancyPanel *panel;
    bool m_visible;

protected:
    void dropEvent(QGraphicsSceneDragDropEvent *event)
    {
        event->setPos(mapToParent(event->pos()));
        panel->dropEvent(event);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget * widget = 0)
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        if (!m_visible) {
            return;
        }

        //TODO: make this a pretty gradient?
        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath p = Plasma::PaintUtils::roundedRectangle(contentsRect().adjusted(1, 1, -2, -2), 4);
        QColor c = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
        c.setAlphaF(0.3);

        painter->fillPath(p, c);
    }
};


FancyPanel::FancyPanel(QObject *parent, const QVariantList &args) : Containment(parent, args),
    m_configureAction(0),
    m_layout(0),
    m_canResize(true),
    m_currentSize(QSize(Kephal::ScreenUtils::screenSize(screen()).width(),35)),
    m_spacerIndex(-1),
    m_spacer(0),
    m_lastSpace(0),
    m_animationTimeLine(new QTimeLine(1000, this)),
    m_maskDirty(true)
{
    KGlobal::locale()->insertCatalog("fancypanel");

    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath("widgets/fancytasks");
    m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    connect(m_background, SIGNAL(repaintNeeded()), this, SLOT(backgroundChanged()));

    setBackgroundHints(NoBackground);

    setZValue(150);

    setObjectName("FancyPanel");

    setSize(QSize(100, 100));
    resize(m_currentSize);

    m_animationTimeLine->setFrameRange(0, 100);
    m_animationTimeLine->setUpdateInterval(50);
    m_animationTimeLine->setCurveShape(QTimeLine::LinearCurve);


    connect(this, SIGNAL(appletRemoved(Plasma::Applet*)),
        this, SLOT(appletRemoved(Plasma::Applet*)));

}

FancyPanel::~FancyPanel()
{
}

void FancyPanel::paintInterface(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           const QRect& contentsRect)
{
    Q_UNUSED(contentsRect)
    //FIXME: this background drawing is bad and ugly =)
    // draw the background untransformed (saves lots of per-pixel-math)
    painter->resetTransform();
//    painter->fillRect(rect(), Qt::transparent);

    const Containment::StyleOption *containmentOpt = qstyleoption_cast<const Containment::StyleOption *>(option);

    QRect viewGeom;
    if (containmentOpt && containmentOpt->view) {
        viewGeom = containmentOpt->view->geometry();
    }

    if (m_maskDirty || m_lastViewGeom != viewGeom) {
        m_maskDirty = false;
        m_lastViewGeom = viewGeom;
        updateBorders(viewGeom);
    }

    // blit the background (saves all the per-pixel-products that blending does)
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->setRenderHint(QPainter::Antialiasing);

    m_background->resizeFrame(boundingRect().size());
    m_background->paintFrame(painter);

}


void FancyPanel::init()
{
    setContainmentType(Containment::PanelContainment);

    Containment::init();

    m_layout = new QGraphicsLinearLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    setLayout(m_layout);

    updateBorders(geometry().toRect());

    setContentsMargins(0, 0, 0, 0);

    KConfigGroup cg = config("Configuration");
    m_currentSize = cg.readEntry("minimumSize", m_currentSize);
    if(formFactor() == Plasma::Vertical) {
    m_currentSize.expandedTo(QSize(0,35));
    } else {
    m_currentSize.expandedTo(QSize(35,0));
    }
    setMinimumSize(cg.readEntry("minimumSize", m_currentSize));
    setMaximumSize(cg.readEntry("maximumSize", m_currentSize));

    constraintsEvent(Plasma::LocationConstraint);

    setDrawWallpaper(false);
    m_lastSpaceTimer = new QTimer(this);
    m_lastSpaceTimer->setSingleShot(true);
    connect(m_lastSpaceTimer, SIGNAL(timeout()),
        this, SLOT(adjustLastSpace));
}

void FancyPanel::updateBorders(const QRect &geom, bool themeChange)
{
    Plasma::Location loc = location();
    FrameSvg::EnabledBorders enabledBorders = FrameSvg::AllBorders;
    qreal topHeight = m_background->marginSize(Plasma::TopMargin);
    qreal bottomHeight = m_background->marginSize(Plasma::BottomMargin);
    qreal leftWidth = m_background->marginSize(Plasma::LeftMargin);
    qreal rightWidth = m_background->marginSize(Plasma::RightMargin);

    int s = screen();
    if(s >= 0) {
    if(loc == BottomEdge || loc == TopEdge) {
        QRect r = Kephal::ScreenUtils::screenGeometry(s);
        enabledBorders ^= FrameSvg::BottomBorder;
        enabledBorders ^= FrameSvg::TopBorder;
        if(loc == BottomEdge) {
            bottomHeight = 0;
            topHeight += m_currentSize.height();
        } else {
            topHeight = 0;
            bottomHeight += m_currentSize.height();
        }
        if(geom.x() <= r.x()) {
            leftWidth = 0;
        }
        if(geom.right() >= r.right()) {
            rightWidth = 0;
        }
    } else if(loc == LeftEdge || loc == RightEdge){
        QRect r = Kephal::ScreenUtils::screenGeometry(s);
        enabledBorders ^= FrameSvg::LeftBorder;
        enabledBorders ^= FrameSvg::RightBorder;
        if(loc == LeftEdge) {
            leftWidth = 0;
            rightWidth += m_currentSize.width();
        } else {
            rightWidth = 0;
            leftWidth += m_currentSize.width();
        }
        if(geom.y() <= r.y()) {
            topHeight = 0;
        }
        if(geom.bottom() >= r.bottom()) {
            bottomHeight = 0;
        }
    } else {
        kDebug() << "No location???";
    }
    }

    m_background->setEnabledBorders(enabledBorders);
    m_background->getMargins(leftWidth, topHeight, rightWidth, bottomHeight);

    const QGraphicsItem *box = toolBox();
    if (box && immutability() == Mutable) {
        QSizeF s = box->boundingRect().size();
        if (formFactor() == Vertical) {
            //hardcoded extra margin for the toolbox right now
        bottomHeight += s.height();
            //Default to horizontal for now
        } else {
            rightWidth += s.width();
        }
    }
    if(m_layout) {
        switch (location()) {
        case LeftEdge:
            rightWidth = qMin(rightWidth, qMax(qreal(1), size().width() - KIconLoader::SizeMedium));
            break;
        case RightEdge:
            leftWidth = qMin(leftWidth, qMax(qreal(1), size().width() - KIconLoader::SizeMedium));
            break;
        case TopEdge:
            bottomHeight = qMin(bottomHeight, qMax(qreal(1), size().height() - KIconLoader::SizeMedium));
            break;
        case BottomEdge:
            topHeight = qMin(topHeight, qMax(qreal(1), size().height() - KIconLoader::SizeMedium));
            break;
        default:
            break;
        }
        m_layout->setContentsMargins(leftWidth, topHeight, rightWidth,bottomHeight);
            m_layout->invalidate();
    }
    resize(preferredSize());

    update();
}


void FancyPanel::backgroundChanged()
{
    constraintsEvent(Plasma::LocationConstraint);
}

void FancyPanel::adjustLastSpace()
{
    if(!m_layout) {
        return;
    }
    bool useSpacer = true;
    if (formFactor() == Plasma::Vertical) {
        foreach (Applet *applet, applets()) {
            if (applet->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
                useSpacer = false;
                break;
            }
        }
    } else {
        foreach (Applet *applet, applets()) {
            if (applet->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag) {
                useSpacer = false;
                break;
            }
        }
    }
    if(useSpacer) {
        if(!m_lastSpace) {
            m_lastSpace = new Spacer(this);
            m_lastSpace->panel = this;
            m_lastSpace->m_visible = false;
            m_lastSpace->setPreferredSize(0,0);
            m_lastSpace->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_layout->removeItem(m_lastSpace);
            delete m_lastSpace;
            m_lastSpace = 0;
        }
    }
}

void FancyPanel::enableUpdateSize()
{
    m_canResize = true;
}

void FancyPanel::layoutApplet(Plasma::Applet* applet, const QPointF &pos)
{
    if(!m_layout) {
        return;
    }
    Plasma::FormFactor f = formFactor();
    int insertIndex =  -1;
    QSizeF appletHint = applet->preferredSize();
    QSizeF panelHint  = layout()->preferredSize();
    if(f == Plasma::Horizontal) {
        if(panelHint.width() + appletHint.width() > size().width()) {
            resize(panelHint.width() + appletHint.width(), size().height());

        }
    } else {
        if(panelHint.height() + appletHint.height() > size().height()) {
            resize(size().width(), panelHint.height() + appletHint.height());
        }
    }
    m_layout->setMaximumSize(size());
    if(pos != QPoint(-1, -1)) {
        insertIndex = getInsertIndex(formFactor(), pos);
    }
    m_layout->removeItem(m_lastSpace);
    if(insertIndex == -1 || insertIndex >= m_layout->count()) {
        m_layout->addItem(applet);
    } else {
        m_layout->insertItem(insertIndex, applet);
    }
    m_lastSpaceTimer->start(2000);
    connect(applet, SIGNAL(sizeHintChanged(Qt::SizeHint)),
        this, SLOT(updateSize));
}

void FancyPanel::appletRemoved(Plasma::Applet *applet) {
    if(!m_layout) {
        return;
    }
    connect(applet, SIGNAL(sizeHintChanged(Qt::SizeHint)), 
        this, SLOT(updateSize()));
    m_layout->removeItem(applet);
    if(formFactor() == Plasma::Horizontal) {
        resize(size().width() - applet->size().width(), size().height());
    } else { // Plasma::Vertical
        resize(size().width(),size().height() - applet->size().height());
    }
    m_layout->setMaximumSize(size());
    m_lastSpaceTimer->start(200);
}

void FancyPanel::updateSize()
{
    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(sender());

    if(m_canResize && applet) {
        if(formFactor() == Plasma::Horizontal) {
            const int delta = applet->preferredWidth() - applet->size().width();
            setPreferredWidth(preferredWidth() + delta);
        } else if(formFactor() == Plasma::Vertical) {
            const int delta = applet->preferredHeight() - applet->size().height();
            setPreferredHeight(preferredHeight() + delta);
        }
        resize(preferredSize());
        m_canResize = false;
        QTimer::singleShot(400, this, SLOT(enableUpdateSize()));
    }
}

void FancyPanel::constraintsEvent(Plasma::Constraints constraints)
{
    m_maskDirty = true;
    if(constraints & Plasma::FormFactorConstraint) {
    Plasma::FormFactor form = formFactor();
    Qt::Orientation layoutDirection = form == Plasma::Vertical ?
        Qt::Vertical : Qt::Horizontal;
    if(m_layout) {
        m_layout->setMaximumSize(size());
        m_layout->setOrientation(layoutDirection);
    }
    }
    if (m_layout && (constraints & Plasma::SizeConstraint)) {
        m_layout->setMaximumSize(size());
    //m_background->setElementPrefix(location());
    m_background->resizeFrame(m_currentSize);
    }

    if (constraints & Plasma::LocationConstraint) {
    switch(location()) {
        case BottomEdge:
        case TopEdge:
            setFormFactor(Plasma::Horizontal);
            break;
        case RightEdge:
        case LeftEdge:
            setFormFactor(Plasma::Vertical);
            break;
        case Floating:
            kDebug() << "Floating is unimplemented";
            break;
        default:
            kDebug() << "Invalid Location!!!";
    }
    m_background->resizeFrame(m_currentSize);
    }

    if(constraints & Plasma::StartupCompletedConstraint) {
    m_background->resizeFrame(m_currentSize);
    connect(this, SIGNAL(appletAdded(Plasma::Applet*,QPointF)),
        this, SLOT(layoutApplet(Plasma::Applet*, QPointF)));
    }

    if(constraints & Plasma::ImmutableConstraint) {
    bool unlocked = immutability() == Plasma::Mutable;
    m_configureAction->setEnabled(unlocked);
    m_configureAction->setVisible(unlocked);
    }

//    setBackgroundHints(NoBackground);

    enableAction("add widgets", true);
    enableAction("add space", true);
}

void FancyPanel::saveState(KConfigGroup &config) const
{
    config.writeEntry("minimumSize", minimumSize());
    config.writeEntry("maximumSize", maximumSize());
}

void FancyPanel::showDropZone(const QPoint pos)
{
    if(!scene() || !m_layout) {
        return;
    }
    if(pos == QPoint()) {
        if(m_spacer) {
            m_layout->removeItem(m_spacer);
            m_spacer->hide();
        }
        return;
    }
    if(m_spacer && m_spacer->geometry().contains(pos)) {
        return;
    }
    int insertIndex = getInsertIndex(formFactor(), pos);
    m_spacerIndex = insertIndex;

    if(insertIndex != -1) {
        if(!m_spacer) {
            m_spacer = new Spacer(this);
            m_spacer->panel = this;
        } else {
            m_layout->removeItem(m_spacer);
        }
        m_spacer->show();
        m_layout->insertItem(insertIndex, m_spacer);
    }
}

void FancyPanel::restore(KConfigGroup &group)
{
    Containment::restore(group);
    KConfigGroup appletsConfig(&group, "Applets");
    QMap<int, Applet *> orderedApplets;
    QList<Applet *> unorderedApplets;
    foreach (Applet *applet, applets()) {
        KConfigGroup appletConfig(&appletsConfig, QString::number(applet->id()));
        KConfigGroup m_layoutConfig(&appletConfig, "LayoutInformation");

        int order = m_layoutConfig.readEntry("Order", -1);
        if(order > -1) {
            orderedApplets[order] = applet;
        } {
            unorderedApplets.append(applet);
        }
        connect(applet, SIGNAL(sizeHintChanged(Qt::SizeHint)),
            this, SLOT(updateSize()));
    }
    foreach (Applet *applet, orderedApplets) {
        if(m_lastSpace) {
            m_layout->insertItem(m_layout->count() - 1, applet);
        } else {
            m_layout->addItem(applet);
        }
    }

    foreach (Applet *applet, unorderedApplets) {
        layoutApplet(applet, applet->pos());
    }
    updateSize();
}

void FancyPanel::saveContents(KConfigGroup &group) const
{
    Containment::saveContents(group);
    KConfigGroup appletsConfig(&group, "Applets");

    for(int order = 0; order < m_layout->count(); ++order) {
        const Applet *applet = dynamic_cast<Applet *>(m_layout->itemAt(order));
        if(applet) {
            KConfigGroup appletConfig(&appletsConfig, QString::number(applet->id()));
            KConfigGroup layoutConfig(&appletConfig, "LayoutInformation");
            layoutConfig.writeEntry("Order", order);
        }
    }
}

int FancyPanel::getInsertIndex(Plasma::FormFactor f, const QPointF &pos)
{
    int insertIndex = m_layout->count();

    for (int i = 0; i < m_layout->count(); ++i) {
        QRectF siblingGeometry = m_layout->itemAt(i)->geometry();

        if (f == Plasma::Horizontal) {
            qreal middle = siblingGeometry.left() + (siblingGeometry.width() / 2.0);
            if (pos.x() < middle) {
                insertIndex = i;
                break;
            } else if (pos.x() <= siblingGeometry.right()) {
                insertIndex = i + 1;
                break;
            }
        } else { // Plasma::Vertical
            qreal middle = siblingGeometry.top() + (siblingGeometry.height() / 2.0);
            if (pos.y() < middle) {
                insertIndex = i;
                break;
            } else if (pos.y() <= siblingGeometry.bottom()) {
                insertIndex = i + 1;
                break;
            }
        }
    }
    return insertIndex;
}

QList<QAction*> FancyPanel::contextualActions()
{
    if(!m_configureAction) {
        m_configureAction = new QAction(i18n("Fancy Panel Settings"), this);
        m_configureAction->setIcon(KIcon("configure"));
        connect(m_configureAction, SIGNAL(triggered()),
            this, SIGNAL(toolBoxToggled()));
        constraintsEvent(Plasma::ImmutableConstraint);
    }
    QList<QAction*> actions;
    actions.append(m_configureAction);

    return actions;
}

void FancyPanel::setSize(QSize size)
{
    if (size.width() > QApplication::desktop()->screenGeometry().size().width() || size.height() > QApplication::desktop()->screenGeometry().size().height())
    {
        if (formFactor() == Plasma::Vertical)
        {
            size = QSize((QApplication::desktop()->screenGeometry().size().width() / 10), (QApplication::desktop()->screenGeometry().size().height() / 3));
        }
        else
        {
            size = QSize((QApplication::desktop()->screenGeometry().size().width() / 3), (QApplication::desktop()->screenGeometry().size().height() / 10));
        }
    }
    setMinimumSize(size);
    setMaximumSize(size);
}

/* 
Plasma::Applet* FancyPanel::addApplet(const QString &name, const QVariantList &args, const QRectF &geometry)
{
    Q_UNUSED(name)
    Q_UNUSED(args)
    Q_UNUSED(geometry)

    return NULL;
}

*/

K_EXPORT_PLASMA_APPLET(fancypanel, FancyPanel)

#include "FancyPanel.moc"
