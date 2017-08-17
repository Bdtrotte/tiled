

#pragma once

#include <QAbstractItemModel>
#include <QDialog>
#include <QGraphicsView>

class QGraphicsScene;
class QListView;
class QUndoStack;

namespace Ui {

class CustomShapeDialog;

}

namespace Tiled {

class Map;

namespace Internal {

class PolygonEditView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit PolygonEditView(QWidget *parent = nullptr);

    void setCurrentPolygon(const QPolygonF &polygon);

    void setEdit(bool value);

signals:
    void polygonChanged(QPolygonF polygon);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int closestPointToPolygon(QPointF point);

    QGraphicsPolygonItem *mPolygon;
    QGraphicsEllipseItem *mEllipse;
    bool mHasInitialized;
    bool mIsEditing;
    bool mMouseHeld;

    //drawing the initial shape. When true, points will be appended
    //to the polygon instead of inserting based on mouse pos.
    bool mInitialShape;
    int mFocusPoint;
};

class ShapeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ShapeModel(Map *map,
                        QObject *parent = nullptr);

    QModelIndex index(int row,
                      int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void insertShape(int index, QPolygonF polygon);
    void replaceShape(int index, QPolygonF polygon);
    void removeShape(int index);

    Map *map() { return mMap; }

private:
    Map *mMap;
};

class CustomShapeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomShapeDialog(Map *map, QWidget *parent = nullptr);
    ~CustomShapeDialog();

signals:
    void shapeSelected(QPolygonF polygon);

private slots:
    void addShape();
    void removeShape();
    void clearShape();
    void editShape(bool value);
    void currentShapeChanged();
    void shapeEdited(QPolygonF polygon);
    void select();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Map *mMap;
    Ui::CustomShapeDialog *mUi;
    ShapeModel *mShapeModel;
    PolygonEditView *mPolygonEditView;
    QListView *mShapeView;
    QUndoStack *mUndoStack;
};

} // namespace Internal
} // namespace Tiled
