#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QList>
#include <QPoint>
#include <QTimer>
#include <QScrollArea>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>

class Shape;
class CompositeShape;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void refreshTimerTick();

private:
    void setupUI();
    int m_activeFocusHandle;
    bool m_isDraggingFocus;
    Shape* copyShapeRecursive(Shape* shape);
    void createShapeSelectionPanel();
    void createPropertiesPanel();
    void createCloseButton();
    void addToShapeList(Shape* shape);
    void updateShapesList();
    void onShapeSelected(Shape* shape);
    void updatePropertiesPanel();
    void updateSideWidgets();
    void updateVertexCoordinates();
    void hidePropertiesPanel();
    void clearAll();
    void loadShapesFromFile();
    void saveSelectedShapesToFile();
    void loadExternalPlugin();
    void createPolygonDialog();
    void createEllipseDialog();
    void startCompositeCreation();
    void cancelCompositeCreation();
    void finishCompositeCreation();
    void showCompositeNameDialog();
    void ungroupCompositeShape();
    void closeApplication();
    int getNextId();
    
    QPoint getCenterPosition() const;
    
    QList<Shape*> m_shapes;
    Shape* m_currentShape;
    int m_nextId;
    
    bool m_isDragging;
    bool m_isDraggingAnchor;
    bool m_isResizing;
    int m_activeResizeHandle;
    QPoint m_lastMousePos;
    QPoint m_resizeStartPos;
    
    QTimer* m_refreshTimer;
    bool m_needsRedraw;
    
    QWidget* m_shapeSelectionPanel;
    QScrollArea* m_propertiesScrollArea;
    QWidget* m_propertiesPanel;
    bool m_isPropertiesVisible;
    QPushButton* m_closeButton;
    QWidget* m_shapesContainer;
    QTabWidget* m_sideTabs;
    QList<QWidget*> m_sidePages;
    QTableWidget* m_vertexTable;
    
    bool m_isCreatingComposite;
    QList<Shape*> m_selectedForComposite;
    QPushButton* m_cancelCompositeBtn;
    QPushButton* m_finishCompositeBtn;
};

#endif