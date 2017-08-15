#include "customshapedialog.h"
#include "ui_customshapedialog.h"

#include "map.h"
#include "utils.h"

#include <QBoxLayout>
#include <QGraphicsView>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QToolBar>

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

class ShapeModel : public QAbstractItemModel
{
public:
    explicit ShapeModel(Map *map,
                        QObject *parent = nullptr)
        : QAbstractItemModel(parent)
        , mMap(map)
    {}

    QModelIndex index(int row,
                      int column,
                      const QModelIndex &parent = QModelIndex()) const override
    {
        if (row >= mMap->shapes().size()
                || parent.isValid()
                || column != 0)
            return QModelIndex();

        return createIndex(row, 0, nullptr);
    }

    QModelIndex parent(const QModelIndex &) const override
    {
        return QModelIndex();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid())
            return 0;

        return mMap->shapes().size();
    }

    int columnCount(const QModelIndex &) const override
    {
        return 1;
    }

    QVariant data(const QModelIndex &index, int role) const override
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

private:
    Map *mMap;
};

namespace Tiled {
namespace Internal {

class PolygonEditView : public QGraphicsView
{
public:
    PolygonEditView(QWidget *parent = nullptr)
        : QGraphicsView(parent)
        , mBrush(Qt::white)
        , mPen(QPen(Qt::black, 3))
    {
    }

    void setCurrentPolygon(QPolygonF polygon);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPolygonF mPolygon;
    QBrush mBrush;
    QPen mPen;
};

}
}

void PolygonEditView::setCurrentPolygon(QPolygonF polygon)
{
    mPolygon = polygon;

    scene()->clear();
    scene()->addPolygon(translatedPolygon(mPolygon, frameRect().adjusted(4, 4, -4, -4)),
                        mPen,
                        mBrush);
}

void PolygonEditView::mouseMoveEvent(QMouseEvent *event)
{

}

void PolygonEditView::mousePressEvent(QMouseEvent *event)
{

}

void PolygonEditView::mouseReleaseEvent(QMouseEvent *event)
{

}

CustomShapeDialog::CustomShapeDialog(Map *map, QWidget *parent)
    : QDialog(parent)
    , mUi(new Ui::CustomShapeDialog)
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

    mShapeView->setModel(new ShapeModel(map, this));

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
    connect(mUi->editShape, &QPushButton::clicked,
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
    mPolygonEditView->setMouseTracking(true);
    mPolygonEditView->show();

    graphicsScene->setBackgroundBrush(Qt::gray);
}

CustomShapeDialog::~CustomShapeDialog()
{
    delete mUi;
}

void CustomShapeDialog::addShape()
{

}

void CustomShapeDialog::removeShape()
{

}

void CustomShapeDialog::clearShape()
{

}

void CustomShapeDialog::editShape()
{

}

void CustomShapeDialog::currentShapeChanged()
{
    const QItemSelectionModel *selectionModel = mShapeView->selectionModel();
    const QModelIndex &index = selectionModel->currentIndex();

    mPolygonEditView->setCurrentPolygon(index.data(Qt::UserRole).value<QPolygonF>());
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
