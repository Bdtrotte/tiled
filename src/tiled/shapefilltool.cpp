/*
 * shapefilltool.cpp
 * Copyright 2017, Benjamin Trotter <bdtrotte@ucsc.edu>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "shapefilltool.h"

#include "addremovetileset.h"
#include "brushitem.h"
#include "customshapedialog.h"
#include "geometry.h"
#include "mapdocument.h"
#include "painttilelayer.h"
#include "stampactions.h"

#include <QApplication>
#include <QActionGroup>
#include <QToolBar>

using namespace Tiled;
using namespace Internal;

ShapeFillTool::ShapeFillTool(QObject *parent)
    : AbstractTileFillTool(tr("Shape Fill Tool"),
                           QIcon(QLatin1String(
                                     ":images/22x22/rectangle-fill.png")),
                           QKeySequence(tr("P")),
                           nullptr,
                           parent)
    , mToolBehavior(Free)
    , mCurrentShape(Rect)
    , mRectFill(new QAction(this))
    , mCircleFill(new QAction(this))
    , mCustomFill(new QAction(this))
{
    QIcon rectFillIcon(QLatin1String(":images/22x22/rectangle-fill.png"));
    QIcon circleFillIcon(QLatin1String(":images/22x22/ellipse-fill.png"));
    QIcon customFillIcon(QLatin1String(":images/22x22/polygon-fill.png"));

    mRectFill->setIcon(rectFillIcon);
    mRectFill->setCheckable(true);
    mRectFill->setChecked(true);

    mCircleFill->setIcon(circleFillIcon);
    mCircleFill->setCheckable(true);

    mCustomFill->setIcon(customFillIcon);
    mCustomFill->setCheckable(true);

    connect(mRectFill, &QAction::triggered,
            [this] { setCurrentShape(Rect); });
    connect(mCircleFill, &QAction::triggered,
            [this] { setCurrentShape(Circle); });
    connect(mCustomFill, &QAction::triggered,
            [this] { setCurrentShape(Custom); });

    languageChanged();
}

void ShapeFillTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (mToolBehavior != Free)
        return;

    if (event->button() == Qt::LeftButton) {
        QPoint pos = tilePosition();
        TileLayer *tileLayer = currentTileLayer();
        if (!tileLayer)
            return;

        mStartCorner = pos;
        mToolBehavior = MakingShape;
    }
}

void ShapeFillTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (mToolBehavior != MakingShape)
        return;

    if (event->button() == Qt::LeftButton) {
        mToolBehavior = Free;

        TileLayer *tileLayer = currentTileLayer();
        if (!tileLayer)
            return;

        if (!brushItem()->isVisible() || !tileLayer->isUnlocked())
            return;

        const TileLayer *preview = mFillOverlay.data();
        if (!preview)
            return;

        PaintTileLayer *paint = new PaintTileLayer(mapDocument(),
                                                   tileLayer,
                                                   preview->x(),
                                                   preview->y(),
                                                   preview);

        paint->setText(QCoreApplication::translate("Undo Commands", "Shape Fill"));

        if (!mMissingTilesets.isEmpty()) {
            for (const SharedTileset &tileset : mMissingTilesets) {
                if (!mapDocument()->map()->tilesets().contains(tileset))
                    new AddTileset(mapDocument(), tileset, paint);
            }

            mMissingTilesets.clear();
        }

        QRegion fillRegion(mFillRegion);
        mapDocument()->undoStack()->push(paint);
        emit mapDocument()->regionEdited(fillRegion, currentTileLayer());

        mFillRegion = QRegion();
        mFillOverlay.clear();
        brushItem()->clear();
    }
}

void ShapeFillTool::modifiersChanged(Qt::KeyboardModifiers)
{
    if (mToolBehavior == MakingShape)
        updateFillOverlay();
}

void ShapeFillTool::languageChanged()
{
    setName(tr("Shape Fill Tool"));
    setShortcut(QKeySequence(tr("P")));

    mRectFill->setToolTip(tr("Rectangle Fill"));
    mCircleFill->setToolTip(tr("Circle Fill"));
    mCustomFill->setToolTip(tr("Select Custom Shape"));

    mStampActions->languageChanged();
}

void ShapeFillTool::populateToolBar(QToolBar *toolBar)
{
    AbstractTileFillTool::populateToolBar(toolBar);

    QActionGroup *actionGroup = new QActionGroup(toolBar);
    actionGroup->addAction(mRectFill);
    actionGroup->addAction(mCircleFill);
    actionGroup->addAction(mCustomFill);

    toolBar->addSeparator();
    toolBar->addActions(actionGroup->actions());
}

void ShapeFillTool::tilePositionChanged(const QPoint&)
{
    if (mToolBehavior == MakingShape)
        updateFillOverlay();
}

void ShapeFillTool::setCurrentShape(Shape shape)
{
    if (shape != Custom) {
        mCurrentShape = shape;
        return;
    }

    CustomShapeDialog dialog(mapDocument()->map());

    connect(&dialog, &CustomShapeDialog::shapeSelected,
            this, &ShapeFillTool::onShapeSelected);

    dialog.exec();

    if (mCurrentShape != Custom) {
        mCurrentShape = Rect;
        mRectFill->setChecked(true);
    }
}

void ShapeFillTool::onShapeSelected(QPolygonF polygon)
{
    if (polygon.isEmpty()) {
        mRectFill->setChecked(true);
        mCurrentShape = Rect;
    } else {
        mCurrentPolygon = polygon;
        mCurrentShape = Custom;
    }
}

void ShapeFillTool::updateFillOverlay()
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer)
        return;

    int width = tilePosition().x() - mStartCorner.x();
    int height = tilePosition().y() - mStartCorner.y();

    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        int min = std::min(abs(width), abs(height));
        width = ((width > 0) - (width < 0))*min;
        height = ((height > 0) - (height < 0))*min;
    }

    int left = std::min(mStartCorner.x(), mStartCorner.x() + width);
    int top = std::min(mStartCorner.y(), mStartCorner.y() + height);

    QRect boundingRect(left, top, abs(width), abs(height));

    switch (mCurrentShape) {
    case Rect:
        mFillRegion = QRegion(boundingRect, QRegion::Rectangle);
        break;
    case Circle:
        mFillRegion = ellipseRegion(boundingRect.center().x(),
                                    boundingRect.center().y(),
                                    boundingRect.right(),
                                    boundingRect.bottom());
        break;
    case Custom:
        mFillRegion = QRegion(transformedPolygon().toPolygon());
        break;
    }

    const QRect fillBound = mFillRegion.boundingRect();
    mFillOverlay = SharedTileLayer(new TileLayer(QString(),
                                                 fillBound.x(),
                                                 fillBound.y(),
                                                 fillBound.width(),
                                                 fillBound.height()));

    if (mIsRandom) {
        randomFill(*mFillOverlay, mFillRegion);
    } else if (mIsWangFill) {
        wangFill(*mFillOverlay, *tileLayer, mFillRegion);
    } else if (mStamp.variations().size() >= 1) {
        fillWithStamp(*mFillOverlay,
                      mStamp,
                      mFillRegion.translated(-mFillOverlay->position()));
    }

    brushItem()->setTileLayer(mFillOverlay);
}

QPolygonF ShapeFillTool::transformedPolygon() const
{
    if (mCurrentPolygon.isEmpty())
        return QPolygon();

    QRectF boundingRect = mCurrentPolygon.boundingRect();

    QPolygonF polygon = mCurrentPolygon;
    polygon.translate(-boundingRect.topLeft());

    QPoint pos = tilePosition();

    qreal xScale = (qreal) (pos.x() - mStartCorner.x()) / boundingRect.width();
    qreal yScale = (qreal) (pos.y() - mStartCorner.y()) / boundingRect.height();

    QTransform t;

    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        qreal scale = std::min(qAbs(xScale), qAbs(yScale));
        qreal xSign = (xScale > 0) - (0 > xScale);
        qreal ySign = (yScale > 0) - (0 > yScale);
        t.scale(xSign * scale, ySign * scale);
    } else {
        t.scale(xScale, yScale);
    }

    return t.map(polygon).translated(mStartCorner);
}
