/*
 * wangset.cpp
 * Copyright 2017, Benjamin Trotter <bdtrotte@ucsc.edu>
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wangset.h"
#include "tilelayer.h"

#include <QStack>
#include <QPoint>

using namespace Tiled;

WangSet::WangSet(Tileset *tileset,
                 int edgeColors,
                 int cornerColors,
                 QString name,
                 int imageTileId):
    Object(Object::WangSetType),
    mTileset(tileset),
    mName(std::move(name)),
    mImageTileId(imageTileId),
    mEdgeColors(edgeColors),
    mCornerColors(cornerColors)
{
}

void WangSet::addTile(Tile *tile, WangId wangId)
{
    Q_ASSERT(tile->tileset() == mTileset);

    for (int i = 0; i < 4; ++i) {
        Q_ASSERT(wangId.edgeColor(i) <= mEdgeColors);
    }

    for (int i = 0; i < 4; ++i) {
        Q_ASSERT(wangId.cornerColor(i) <= mCornerColors);
    }

    mWangIdToTile.insert(wangId, tile);
    mTileIdToWangId.insert(tile->id(), wangId);
}

Tile *WangSet::findMatchingTile(WangId wangId) const
{
    auto potentials = findMatchingTiles(wangId);

    if (potentials.length() > 0)
        return potentials[qrand() % potentials.length()];
    else
        return NULL;
}

struct WangWildCard
{
    int index, colorCount;
};

QList<Tile*> WangSet::findMatchingTiles(WangId wangId) const
{
    QList<Tile*> list;

    //Stores the space of a wild card, followed by how many colors that space can have.
    QVector<WangWildCard> wildCards;

    if (mEdgeColors > 0) {
        for (int i = 0; i < 4; ++i) {
            if (!wangId.edgeColor(i)) {
                WangWildCard w;
                w.index = i * 8;
                w.colorCount = mEdgeColors;

                wildCards.append(w);
            }
        }
    }

    if (mCornerColors > 0) {
        for (int i = 0; i < 4; ++i) {
            if (!wangId.cornerColor(i)) {
                WangWildCard w;
                w.index = i * 8 + 4;
                w.colorCount = mCornerColors;

                wildCards.append(w);
            }
        }
    }

    if (wildCards.isEmpty()) {
        list.append(mWangIdToTile.values(wangId));
    } else {
        QStack<WangWildCard> stack;

        stack += wildCards;

        int max = wildCards.size();

        while (!stack.isEmpty()) {
            if (stack.size() == max) {
                int idVariation = 0;

                for (int i = 0; i < max; ++i)
                    idVariation |= stack[i].colorCount << stack[i].index;

                list.append(mWangIdToTile.values(idVariation | wangId));

                WangWildCard top = stack.pop();
                top.colorCount -= 1;
                if (top.colorCount > 0)
                    stack.push(top);
            } else {
                WangWildCard top = stack.pop();
                top.colorCount -= 1;
                if (top.colorCount > 0) {
                    stack.push(top);

                    for (int i = stack.size(); i < max; ++i)
                        stack.push(wildCards[i]);
                }
            }
        }
    }

    return list;
}

//Pretty sloppy function at the moment, will be condenced in the final version
//Also needs to take into account surrouding tiles, not just those in the layer its filling.
WangId WangSet::wangIdFromSurroundings(TileLayer *tileLayer, int x, int y)
{
    unsigned id = 0;

    if (mCornerColors > 0) {
        unsigned cornerId = 0;

        if (tileLayer->contains(x+1,y-1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x+1,y-1)).tile());
            cornerId &= 0x00f00000;
            id |= cornerId >> 16;
        }
        if (cornerId == 0 && tileLayer->contains(x, y-1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y-1)).tile());
            cornerId &= 0x0000f000;
            id |= cornerId >> 8;
        }
        if (cornerId == 0 && tileLayer->contains(x+1, y)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x+1,y)).tile());
            cornerId &= 0xf0000000;
            id |= cornerId >> 24;
        }

        cornerId = 0;

        if (tileLayer->contains(x+1,y+1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x+1,y+1)).tile());
            id |= (cornerId & 0xf0000000) >> 16;
        }
        if (cornerId == 0 && tileLayer->contains(x+1, y)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x+1,y)).tile());
            cornerId &= 0x00f00000;
            id |= cornerId >> 8;
        }
        if (cornerId == 0 && tileLayer->contains(x, y+1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y+1)).tile());
            cornerId &= 0x000000f0;
            id |= cornerId << 8;
        }

        cornerId = 0;

        if (tileLayer->contains(x-1,y+1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x-1,y+1)).tile());
            id |= (cornerId & 0x000000f0) << 16;
        }
        if (cornerId == 0 && tileLayer->contains(x, y+1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y+1)).tile());
            cornerId &= 0xf0000000;
            id |= cornerId >> 8;
        }
        if (cornerId == 0 && tileLayer->contains(x-1, y)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x-1,y)).tile());
            cornerId &= 0x0000f000;
            id |= cornerId << 8;
        }

        cornerId = 0;

        if (tileLayer->contains(x-1,y-1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x-1,y-1)).tile());
            id |= (cornerId & 0x0000f000) << 16;
        }
        if (cornerId == 0 && tileLayer->contains(x-1, y)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x-1,y)).tile());
            cornerId &= 0x000000f0;
            id |= cornerId << 24;
        }
        if (cornerId == 0 && tileLayer->contains(x, y-1)) {
            cornerId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y-1)).tile());
            cornerId &= 0x00f00000;
            id |= cornerId << 8;
        }
    }
    if (mEdgeColors > 0) {
        unsigned edgeId;
        unsigned mask = 0x0000000f;

        if (tileLayer->contains(x,y-1)) {
            edgeId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y-1)).tile());
            id |= edgeId & mask;
        }
        mask <<= 8;

        if (tileLayer->contains(x+1,y)) {
            edgeId = wangIdOfTile(tileLayer->cellAt(QPoint(x+1,y)).tile());
            id |= edgeId & mask;
        }
        mask <<= 8;

        if (tileLayer->contains(x,y+1)) {
            edgeId = wangIdOfTile(tileLayer->cellAt(QPoint(x,y+1)).tile());
            id |= edgeId & mask;
        }
        mask <<= 8;

        if (tileLayer->contains(x-1,y)) {
            edgeId = wangIdOfTile(tileLayer->cellAt(QPoint(x-1,y)).tile());
            id |= edgeId & mask;
        }
    }

    return WangId(id);
}

WangId WangSet::wangIdOfTile(const Tile *tile) const
{
    if(tile)
        return mTileIdToWangId.value(tile->id());
    else
        return 0;
}

WangSet *WangSet::clone(Tileset *tileset) const
{
    WangSet *c = new WangSet(tileset, mEdgeColors, mCornerColors, mName, mImageTileId);

    c->mWangIdToTile = mWangIdToTile;
    c->mTileIdToWangId = mTileIdToWangId;

    return c;
}
