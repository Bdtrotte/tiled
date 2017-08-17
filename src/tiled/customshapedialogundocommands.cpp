/*
 * customshapedialogundocommands.h
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

#include "customshapedialogundocommands.h"

#include "customshapedialog.h"

#include "map.h"

using namespace Tiled;
using namespace Internal;

AddShape::AddShape(ShapeModel *shapeModel)
    : mShapeModel(shapeModel)
{
}

void AddShape::redo()
{
    mShapeModel->insertShape(mShapeModel->map()->shapes().size(), QPolygonF());
}

void AddShape::undo()
{
    mShapeModel->removeShape(mShapeModel->map()->shapes().size() - 1);
}

RemoveShape::RemoveShape(ShapeModel *shapeModel, int index)
    : mShapeModel(shapeModel)
    , mIndex(index)
{
    mPolygon = mShapeModel->map()->shapes().at(index);
}

void RemoveShape::redo()
{
    mShapeModel->removeShape(mIndex);
}

void RemoveShape::undo()
{
    mShapeModel->insertShape(mIndex, mPolygon);
}

ModifyShape::ModifyShape(ShapeModel *shapeModel, int index, QPolygonF newShape)
    : mShapeModel(shapeModel)
    , mIndex(index)
    , mNewShape(newShape)
{
    mOldShape = mShapeModel->map()->shapes().at(mIndex);
}

void ModifyShape::redo()
{
    mShapeModel->replaceShape(mIndex, mNewShape);
}

void ModifyShape::undo()
{
    mShapeModel->replaceShape(mIndex, mOldShape);
}
