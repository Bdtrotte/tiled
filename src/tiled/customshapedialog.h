

#pragma once

#include <QDialog>

class QGraphicsScene;
class QListView;

namespace Ui {

class CustomShapeDialog;

}

namespace Tiled {

class Map;

namespace Internal {

class PolygonEditView;

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
    void editShape();
    void currentShapeChanged();
    void select();

private:
    Ui::CustomShapeDialog *mUi;
    PolygonEditView *mPolygonEditView;
    QListView *mShapeView;
};

} // namespace Internal
} // namespace Tiled
