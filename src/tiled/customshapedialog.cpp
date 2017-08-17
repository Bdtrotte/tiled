#include "customshapedialog.h"
#include "ui_customshapedialog.h"

#include "customshapedialogundocommands.h"
#include "map.h"
#include "utils.h"

#include <limits>
#include <QBoxLayout>
#include <QGraphicsItem>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QToolBar>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

//translates the polygon so centured the rect
//and it fills out the rect. If lockedScale, then the poly will be evenly
//scaled
static QPolygonF translatedPolygon(QPolygonF polygon,
                                   const QRectF &rect)
{
    QRectF boundingRect = polygon.boundingRect();

    polygon.translate(-boundingRect.left(), -boundingRect.top());

    qreal xScale = (qreal) rect.width() / boundingRect.width();
    qreal yScale = (qreal) rect.height() / boundingRect.height();

    qreal scale = std::min(xScale, yScale);

    QTransform t;
    t.scale(scale, scale);

    polygon = t.map(polygon);
    QPointF center = polygon.boundingRect().center();

    return polygon.translated(rect.center().x() - center.x(),
                              rect.center().y() - center.y());
}

ShapeModel::ShapeModel(Map *map,
                       QObject *parent)
    : QAbstractItemModel(parent)
    , mMap(map)
{
}

QModelIndex ShapeModel::index(int row,
                              int column,
                              const QModelIndex &parent) const
{
    if (row >= mMap->shapes().size()
            || parent.isValid()
            || column != 0)
        return QModelIndex();

    return createIndex(row, 0, nullptr);
}

QModelIndex ShapeModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int ShapeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mMap->shapes().size();
}

int ShapeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant ShapeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    switch(role) {
    case Qt::DecorationRole: {
        QPixmap image(32, 32);
        image.fill(QColor(0,0,0,0));
        QPainter painter;
        painter.begin(&image);
        painter.setBrush(QColor(100, 100, 100, 200));
        painter.setPen(QPen(QColor(100, 100, 100), 1));
        painter.drawPolygon(translatedPolygon(mMap->shapes().at(index.row()),
                                              QRect(1, 1, 30, 30)));
        painter.end();
        return image;
    }
    case Qt::UserRole:
        return mMap->shapes().at(index.row());
    }

    return QVariant();
}

void ShapeModel::insertShape(int index, QPolygonF polygon)
{
    beginInsertRows(QModelIndex(), index, index);
    mMap->insertShape(index, polygon);
    endInsertRows();
}

void ShapeModel::replaceShape(int index, QPolygonF polygon)
{
    mMap->removeShape(index);
    mMap->insertShape(index, polygon);

    QVector<int> roles;
    roles.append(Qt::DecorationRole);

    emit dataChanged(ShapeModel::index(index, 0),
                     ShapeModel::index(index, 0),
                     roles);
}

void ShapeModel::removeShape(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
    mMap->removeShape(index);
    endRemoveRows();
}

PolygonEditView::PolygonEditView(QWidget *parent)
    : QGraphicsView(parent)
    , mPolygon(new QGraphicsPolygonItem)
    , mEllipse(new QGraphicsEllipseItem)
    , mHasInitialized(false)
    , mIsEditing(false)
    , mMouseHeld(false)
    , mInitialShape(true)
    , mFocusPoint(0)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    mPolygon->setBrush(QColor(135, 225, 255, 150));
    mPolygon->setPen(QPen(QColor(48, 132, 160), 3));

    mEllipse->setBrush(Qt::NoBrush);
    mEllipse->setPen(QPen(QColor(74, 125, 142), 2));
    mEllipse->setVisible(false);
}

void PolygonEditView::setCurrentPolygon(const QPolygonF &polygon)
{
    mPolygon->setPolygon(translatedPolygon(polygon, frameRect().adjusted(4, 4, -4, -4)));

    mEllipse->setVisible(!polygon.isEmpty());

    if (polygon.isEmpty()) {
        mFocusPoint = 0;
        mInitialShape = true;
    }

    if (!mHasInitialized) {
        scene()->addItem(mPolygon);
        scene()->addItem(mEllipse);
        mHasInitialized = true;
    }
}

void PolygonEditView::setEdit(bool value)
{
    if (value == mIsEditing)
        return;

    mIsEditing = value;
    mEllipse->setVisible(value);
    setMouseTracking(mIsEditing);
}

void PolygonEditView::mouseMoveEvent(QMouseEvent *event)
{
    if (!mIsEditing
            || mPolygon->polygon().isEmpty()
            || !frameRect().contains(event->pos()))
        return;

    QPolygonF polygon = mPolygon->polygon();

    if (mMouseHeld) {
        polygon[mFocusPoint] = mapToScene(event->pos());
        mPolygon->setPolygon(polygon);

        setSceneRect(frameRect());
    } else {
        mFocusPoint = closestPointToPolygon(mapToScene(event->pos()));
    }

    mEllipse->setRect(QRectF(polygon[mFocusPoint], QSizeF(10, 10)).translated(-5, -5));
}

void PolygonEditView::mousePressEvent(QMouseEvent *event)
{
    if (mMouseHeld)
        return;

    if (!mIsEditing
            || mMouseHeld
            || (event->button() != Qt::LeftButton && event->button() != Qt::RightButton))
        return;

    mMouseHeld = true;

    setSceneRect(frameRect());

    if (event->button() == Qt::LeftButton) {
        QPolygonF polygon = mPolygon->polygon();
        if (polygon.isEmpty())
            return;

        mInitialShape = false;
        polygon[mFocusPoint] = mapToScene(event->pos());
        mPolygon->setPolygon(polygon);

        mEllipse->setRect(QRectF(polygon[mFocusPoint], QSizeF(10, 10)).translated(-5, -5));
    } else {
        QPolygonF polygon = mPolygon->polygon();
        QPointF mousePos = mapToScene(event->pos());
        if (polygon.length() < 3 || mInitialShape) {
            polygon.append(mousePos);
            mFocusPoint = polygon.size() - 1;
            mPolygon->setPolygon(polygon);

            mEllipse->setVisible(true);
            mEllipse->setRect(QRectF(mousePos, QSizeF(10, 10)).translated(-5, -5));
            return;
        }

        //go through and find the line closest to the mouse.
        //this line is point to point + 1
        int point = -1;
        float distance = std::numeric_limits<float>::max();
        for (int i = 0; i < polygon.length(); ++i) {
            QLineF l(polygon[i], polygon[(i + 1) % polygon.length()]);
            QLineF ml(polygon[i], mousePos);

            QLineF ul = l.unitVector();
            float linePos = (ml.dx() * ul.dx()) + (ml.dy() * ul.dy());
            if (linePos < 0 || linePos > l.length())
                continue;

            l = l.normalVector();
            float d = qAbs((l.dx() * ml.dx()) + (l.dy() * ml.dy()));

            if (d < distance) {
                distance = d;
                point = i;
            }
        }

        if (point != -1) {
            polygon.insert(point + 1, mousePos);
            mPolygon->setPolygon(polygon);
            mFocusPoint = point + 1;

            mEllipse->setVisible(true);
            mEllipse->setRect(QRectF(mousePos, QSizeF(10, 10)).translated(-5, -5));
        }
    }

    setSceneRect(frameRect());
}

void PolygonEditView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!mIsEditing)
        return;

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        emit polygonChanged(mPolygon->polygon());
        mMouseHeld = false;
    }
}

void PolygonEditView::keyPressEvent(QKeyEvent *event)
{
    if (mIsEditing
            && !mPolygon->polygon().isEmpty()
            && !mMouseHeld
            && event->key() == Qt::Key_Delete) {
        QPolygonF polygon = mPolygon->polygon();
        polygon.removeAt(mFocusPoint);
        mPolygon->setPolygon(polygon);

        emit polygonChanged(polygon);

        if (polygon.isEmpty()) {
            mEllipse->setVisible(false);
            mFocusPoint = 0;
            mInitialShape = true;
        } else {
            mFocusPoint = closestPointToPolygon(mapToScene(cursor().pos()));
            mEllipse->setRect(QRectF(mPolygon->polygon()[mFocusPoint], QSizeF(10, 10)).translated(-5, -5));
        }
    }
}

int PolygonEditView::closestPointToPolygon(QPointF point)
{
    QPolygonF polygon = mPolygon->polygon();
    if (polygon.isEmpty())
        return -1;

    int p = 0;
    int distanct = (point - polygon[0]).manhattanLength();
    for (int i = 1; i < polygon.length(); ++i) {
        float d = (point - polygon[i]).manhattanLength();
        if (d < distanct) {
            distanct = d;
            p = i;
        }
    }

    return p;
}

CustomShapeDialog::CustomShapeDialog(Map *map, QWidget *parent)
    : QDialog(parent)
    , mMap(map)
    , mUi(new Ui::CustomShapeDialog)
    , mUndoStack(new QUndoStack)
{
    mUi->setupUi(this);

    mShapeView = new QListView;
    mShapeView->setFlow(QListView::LeftToRight);
    mShapeView->setWrapping(true);
    mShapeView->setResizeMode(QListView::Adjust);
    mShapeView->setSelectionMode(QAbstractItemView::SingleSelection);
    mShapeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mShapeView->setUniformItemSizes(true);
    mShapeView->setSpacing(0);

    mShapeModel = new ShapeModel(mMap, this);
    mShapeView->setModel(mShapeModel);

    connect(mShapeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &CustomShapeDialog::currentShapeChanged);

    QAction *addShape = new QAction(this);
    QAction *removeShape = new QAction(this);

    connect(addShape, &QAction::triggered, this, &CustomShapeDialog::addShape);
    connect(removeShape, &QAction::triggered, this, &CustomShapeDialog::removeShape);

    addShape->setIcon(QIcon(QStringLiteral(":/images/22x22/add.png")));
    removeShape->setIcon(QIcon(QStringLiteral(":/images/22x22/remove.png")));

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setFloatable(false);
    toolBar->setMovable(false);
    toolBar->setIconSize(Utils::smallIconSize());

    toolBar->addAction(addShape);
    toolBar->addAction(removeShape);

    QHBoxLayout *horizontal = new QHBoxLayout;
    horizontal->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding));
    horizontal->addWidget(toolBar);

    QVBoxLayout *vertical = new QVBoxLayout(mUi->listArea);
    vertical->addWidget(mShapeView);
    vertical->addLayout(horizontal);

    connect(mUi->selectButton, &QPushButton::clicked,
            this, &CustomShapeDialog::select);
    connect(mUi->editShape, &QPushButton::toggled,
            this, &CustomShapeDialog::editShape);
    connect(mUi->clearShape, &QPushButton::clicked,
            this, &CustomShapeDialog::clearShape);

    QGraphicsScene *graphicsScene = new QGraphicsScene(this);

    mPolygonEditView = new PolygonEditView(mUi->polygonEditArea);

    mPolygonEditView->setScene(graphicsScene);
    mPolygonEditView->setGeometry(0,
                                  0,
                                  mUi->polygonEditArea->width(),
                                  mUi->polygonEditArea->height());
    mPolygonEditView->show();

    connect(mPolygonEditView, &PolygonEditView::polygonChanged,
            this, &CustomShapeDialog::shapeEdited);

    graphicsScene->setBackgroundBrush(Qt::gray);
}

CustomShapeDialog::~CustomShapeDialog()
{
    delete mUi;
    delete mUndoStack;
}

void CustomShapeDialog::addShape()
{
    if (mMap->shapes().isEmpty()
            || !mMap->shapes().at(mMap->shapes().size() - 1).isEmpty())
        mUndoStack->push(new AddShape(mShapeModel));

    QItemSelectionModel *selectionModel = mShapeView->selectionModel();
    QModelIndex lastIndex = mShapeModel->index(mMap->shapes().size() - 1, 0);
    selectionModel->setCurrentIndex(lastIndex, QItemSelectionModel::ClearAndSelect);
    mUi->editShape->setChecked(true);
}

void CustomShapeDialog::removeShape()
{
    QModelIndex index = mShapeView->selectionModel()->currentIndex();

    if (!index.isValid() || index.parent().isValid())
        return;

    mUndoStack->push(new RemoveShape(mShapeModel, index.row()));
}

void CustomShapeDialog::clearShape()
{
    QModelIndex index = mShapeView->selectionModel()->currentIndex();

    if (!index.isValid() || index.parent().isValid())
        return;

    mUndoStack->push(new ModifyShape(mShapeModel, index.row(), QPolygonF()));

    mPolygonEditView->setCurrentPolygon(QPolygonF());
}

void CustomShapeDialog::editShape(bool value)
{
    mPolygonEditView->setEdit(value);
}

void CustomShapeDialog::currentShapeChanged()
{
    const QItemSelectionModel *selectionModel = mShapeView->selectionModel();
    const QModelIndex &index = selectionModel->currentIndex();

    mPolygonEditView->setCurrentPolygon(index.data(Qt::UserRole).value<QPolygonF>());

    mUi->editShape->setChecked(false);
}

void CustomShapeDialog::shapeEdited(QPolygonF polygon)
{
    QModelIndex index = mShapeView->selectionModel()->currentIndex();

    if (!index.isValid() || index.parent().isValid())
        return;

    mUndoStack->push(new ModifyShape(mShapeModel, index.row(), polygon));
}

void CustomShapeDialog::select()
{
    const QItemSelectionModel *selectionModel = mShapeView->selectionModel();
    const QModelIndex &index = selectionModel->currentIndex();

    if (index.isValid())
        emit shapeSelected(index.data(Qt::UserRole).value<QPolygonF>());
    else
        emit shapeSelected(QPolygonF());

    close();
}

void CustomShapeDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Z
            && (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            if (mUndoStack->canRedo())
                mUndoStack->redo();
        } else if (mUndoStack->canUndo()) {
            mUndoStack->undo();
        }

        const QItemSelectionModel *selectionModel = mShapeView->selectionModel();
        const QModelIndex &index = selectionModel->currentIndex();

        if (index.isValid())
            mPolygonEditView->setCurrentPolygon(index.data(Qt::UserRole).value<QPolygonF>());
    }
}
