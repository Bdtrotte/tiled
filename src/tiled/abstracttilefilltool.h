

#pragma once

#include "abstracttiletool.h"
#include "randompicker.h"
#include "tilelayer.h"
#include "tilestamp.h"

namespace Tiled {

class WangSet;

namespace Internal {

class MapDocument;
class StampActions;
class WangFiller;

class AbstractTileFillTool : public AbstractTileTool
{
    Q_OBJECT

public:
    AbstractTileFillTool(const QString &name,
                         const QIcon &icon,
                         const QKeySequence &shortcut,
                         QObject *parent = nullptr,
                         BrushItem *brushItem = nullptr);
    ~AbstractTileFillTool();

    /**
     * Sets the stamp that is drawn when filling.
     */
    void setStamp(const TileStamp &stamp);

    /**
     * This returns the current stamp used for filling.
     */
    const TileStamp &stamp() const { return mStamp; }

    void populateToolBar(QToolBar *toolBar) override;

public slots:
    void setRandom(bool value);
    void setWangFill(bool value);
    void setWangSet(WangSet *wangSet);

signals:
    void stampChanged(const TileStamp &stamp);

    void randomChanged(bool value);

    void wangFillChanged(bool value);

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument) override;

    virtual void clearConnections(MapDocument *mapDocument) = 0;

    /**
     * Fills the given \a region in the given \a tileLayer with random tiles.
     */
    void randomFill(TileLayer &tileLayer, const QRegion &region) const;

    void wangFill(TileLayer &tileLayerToFill,
                  const TileLayer &backgroundTileLayer,
                  const QRegion &region) const;

    void fillWithStamp(TileLayer &layer,
                       const TileStamp &stamp,
                       const QRegion &mask);

    void clearOverlay();

    TileStamp mStamp;
    SharedTileLayer mFillOverlay;
    QRegion mFillRegion;
    QVector<SharedTileset> mMissingTilesets;

    bool mIsRandom;
    bool mIsWangFill;

    /**
     * Contains the value of mIsRandom at that time, when the latest call of
     * tilePositionChanged() took place.
     * This variable is needed to detect if the random mode was changed during
     * mFillOverlay being brushed at an area.
     */
    bool mLastRandomStatus;

    StampActions *mStampActions;

private:
    WangFiller *mWangFiller;
    RandomPicker<Cell> mRandomCellPicker;

    /**
     * Updates the list of random cells.
     * This is done by taking all non-null tiles from the original stamp mStamp.
     */
    void updateRandomListAndMissingTilesets();
};

}
}
