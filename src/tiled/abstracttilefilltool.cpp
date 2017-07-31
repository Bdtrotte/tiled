

#include "abstracttilefilltool.h"
#include "brushitem.h"
#include "mapdocument.h"
#include "staggeredrenderer.h"
#include "stampactions.h"
#include "wangfiller.h"

#include <QAction>

using namespace Tiled;
using namespace Internal;

AbstractTileFillTool::AbstractTileFillTool(const QString &name,
                                           const QIcon &icon,
                                           const QKeySequence &shortcut,
                                           QObject *parent,
                                           BrushItem *brushItem)
    : AbstractTileTool(name, icon, shortcut, parent, brushItem)
    , mIsRandom(false)
    , mIsWangFill(false)
    , mLastRandomStatus(false)
    , mStampActions(new StampActions(this))
    , mWangFiller(new WangFiller(nullptr))
{
    connect(mStampActions->random(), &QAction::toggled, this, &AbstractTileFillTool::randomChanged);
    connect(mStampActions->wangFill(), &QAction::toggled, this, &AbstractTileFillTool::wangFillChanged);

    connect(mStampActions->flipHorizontal(), &QAction::triggered,
            [this]() { emit stampChanged(mStamp.flipped(FlipHorizontally)); });
    connect(mStampActions->flipVertical(), &QAction::triggered,
            [this]() { emit stampChanged(mStamp.flipped(FlipVertically)); });
    connect(mStampActions->rotateLeft(), &QAction::triggered,
            [this]() { emit stampChanged(mStamp.rotated(RotateLeft)); });
    connect(mStampActions->rotateRight(), &QAction::triggered,
            [this]() { emit stampChanged(mStamp.rotated(RotateRight)); });
}

AbstractTileFillTool::~AbstractTileFillTool()
{
}

void AbstractTileFillTool::setStamp(const TileStamp &stamp)
{
    // Clear any overlay that we presently have with an old stamp
    clearOverlay();

    mStamp = stamp;

    updateRandomListAndMissingTilesets();

    if (brushItem()->isVisible())
        tilePositionChanged(tilePosition());
}

void AbstractTileFillTool::populateToolBar(QToolBar *toolBar)
{
    mStampActions->populateToolBar(toolBar, mIsRandom, mIsWangFill);
}

void AbstractTileFillTool::setRandom(bool value)
{
    if (mIsRandom == value)
        return;

    mIsRandom = value;

    if (mIsRandom) {
        mIsWangFill = false;
        mStampActions->wangFill()->setChecked(false);

        updateRandomListAndMissingTilesets();
    }

    // Don't need to recalculate fill region if there was no fill region
    if (!mFillOverlay)
        return;

    tilePositionChanged(tilePosition());
}

void AbstractTileFillTool::setWangFill(bool value)
{
    if (mIsWangFill == value)
        return;

    mIsWangFill = value;

    if (mIsWangFill) {
        mIsRandom = false;
        mStampActions->random()->setChecked(false);

        updateRandomListAndMissingTilesets();
    }

    if (!mFillOverlay)
        return;

    tilePositionChanged(tilePosition());
}

void AbstractTileFillTool::setWangSet(WangSet *wangSet)
{
    mWangFiller->setWangSet(wangSet);

    updateRandomListAndMissingTilesets();
}

void AbstractTileFillTool::mapDocumentChanged(MapDocument *oldDocument,
                                              MapDocument *newDocument)
{
    AbstractTileTool::mapDocumentChanged(oldDocument, newDocument);

    clearConnections(oldDocument);

    if (newDocument)
        updateRandomListAndMissingTilesets();

    clearOverlay();
}

void AbstractTileFillTool::clearOverlay()
{
    brushItem()->clear();
    mFillOverlay.clear();
    mFillRegion = QRegion();
}

void AbstractTileFillTool::updateRandomListAndMissingTilesets()
{
    mRandomCellPicker.clear();
    mMissingTilesets.clear();

    if (!mapDocument())
        return;

    if (mIsWangFill && mWangFiller->wangSet()) {
        const SharedTileset &tileset = mWangFiller->wangSet()->tileset()->sharedPointer();
        if (!mapDocument()->map()->tilesets().contains(tileset))
            mMissingTilesets.append(tileset);
    } else {
        for (const TileStampVariation &variation : mStamp.variations()) {
            mapDocument()->unifyTilesets(variation.map, mMissingTilesets);

            if (mIsRandom) {
                const TileLayer &tileLayer = *variation.tileLayer();
                for (const Cell &cell : tileLayer) {
                    if (const Tile *tile = cell.tile())
                        mRandomCellPicker.add(cell, tile->probability());
                }
            }
        }
    }
}

void AbstractTileFillTool::randomFill(TileLayer &tileLayer, const QRegion &region) const
{
    if (region.isEmpty() || mRandomCellPicker.isEmpty())
        return;

    for (const QRect &rect : region.translated(-tileLayer.position()).rects()) {
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                tileLayer.setCell(x, y,
                                  mRandomCellPicker.pick());
            }
        }
    }
}

void AbstractTileFillTool::wangFill(TileLayer &tileLayerToFill,
                                    const TileLayer &backgroundTileLayer,
                                    const QRegion &region) const
{
    if (region.isEmpty() || !mWangFiller->wangSet())
        return;

    TileLayer *stamp = mWangFiller->fillRegion(backgroundTileLayer,
                                               region,
                                               dynamic_cast<StaggeredRenderer*>(mapDocument()->renderer()),
                                               mapDocument()->map()->staggerAxis());

    tileLayerToFill.setCells(0, 0, stamp);
    delete stamp;
}

void AbstractTileFillTool::fillWithStamp(TileLayer &layer,
                                         const TileStamp &stamp,
                                         const QRegion &mask)
{
    const QSize size = stamp.maxSize();

    // Fill the entire layer with random variations of the stamp
    for (int y = 0; y < layer.height(); y += size.height()) {
        for (int x = 0; x < layer.width(); x += size.width()) {
            const TileStampVariation variation = stamp.randomVariation();
            layer.setCells(x, y, variation.tileLayer());
        }
    }

    // Erase tiles outside of the masked region. This can easily be faster than
    // avoiding to place tiles outside of the region in the first place.
    layer.erase(QRegion(0, 0, layer.width(), layer.height()) - mask);
}
