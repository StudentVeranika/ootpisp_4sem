#include "main_window.h"
#include "shapes/polygon.h"
#include "shapes/ellipse.h"
#include "shapes/composite_shape.h"
#include "factory/shape_factory.h"
#include "factory/shape_loader.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QInputDialog>
#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QListWidget>
#include <QMessageBox>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <cmath>
#include <climits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentShape(nullptr)
    , m_nextId(1)
    , m_isDragging(false)
    , m_isDraggingAnchor(false)
    , m_isResizing(false)
    , m_activeResizeHandle(-1)
    , m_activeFocusHandle(-1)
    , m_isDraggingFocus(false)
    , m_refreshTimer(new QTimer(this))
    , m_needsRedraw(false)
    , m_propertiesPanel(nullptr)
    , m_isPropertiesVisible(false)
    , m_propertiesScrollArea(nullptr)
    , m_closeButton(nullptr)
    , m_shapesContainer(nullptr)
    , m_sideTabs(nullptr)
    , m_vertexTable(nullptr)
    , m_isCreatingComposite(false)
    , m_cancelCompositeBtn(nullptr)
    , m_finishCompositeBtn(nullptr)
{
    setupUI();
    createShapeSelectionPanel();
    createPropertiesPanel();
    
    m_refreshTimer->setInterval(16);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshTimerTick);
    m_refreshTimer->start();
    
    setFocusPolicy(Qt::StrongFocus);
    showFullScreen();
}

MainWindow::~MainWindow()
{
    for (Shape* shape : m_shapes) {
        delete shape;
    }
    m_shapes.clear();
}

void MainWindow::setupUI()
{
    setWindowTitle("Графический редактор");
    setStyleSheet("background-color: white;");
    
    QScreen *screen = QApplication::primaryScreen();
    if (screen) setGeometry(screen->geometry());
}
int MainWindow::getNextId()
{
    return m_nextId++;
}
void MainWindow::createCloseButton()
{
    m_closeButton = new QPushButton("X", this);
    m_closeButton->setFixedSize(40, 40);
    m_closeButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
    m_closeButton->move(width() - 60, 10);
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::closeApplication);
}

void MainWindow::loadShapesFromFile()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Загрузить фигуры",
        QDir::homePath(),
        "JSON files (*.json)");
    
    if (filename.isEmpty()) return;
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл");
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    QJsonArray shapesArray;
    if (doc.isArray()) {
        shapesArray = doc.array();
    } else if (doc.isObject()) {
        shapesArray.append(doc.object());
    } else {
        QMessageBox::critical(this, "Ошибка", "Неверный формат файла");
        return;
    }
    
    if (shapesArray.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Файл не содержит фигур");
        return;
    }
    
    struct ShapeInfo {
        QJsonObject json;
        QString type;
        QString name;
        int id;
        int savedAnchorX;
        int savedAnchorY;
    };
    
    QList<ShapeInfo> shapeInfos;
    
    for (int i = 0; i < shapesArray.size(); ++i) {
        QJsonObject json = shapesArray[i].toObject();
        QString type = json["type"].toString();
        
        if (type != "Polygon" && type != "Ellipse" && type != "Composite") {
            continue;
        }
        
        ShapeInfo info;
        info.json = json;
        info.type = type;
        info.name = json["name"].toString();
        info.id = json["id"].toInt();
        info.savedAnchorX = json["saved_anchor_x"].toInt();
        info.savedAnchorY = json["saved_anchor_y"].toInt();
        
        shapeInfos.append(info);
    }
    
    if (shapeInfos.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "В файле нет корректных фигур");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Выберите фигуры для загрузки");
    dialog.setModal(true);
    dialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel(QString("Файл: %1\nНайдено фигур: %2\n\nВыберите фигуры для загрузки:")
        .arg(filename).arg(shapeInfos.size()));
    label->setWordWrap(true);
    layout->addWidget(label);
    
    QListWidget* listWidget = new QListWidget();
    listWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    
    QHash<QListWidgetItem*, ShapeInfo> itemToInfo;
    
    for (const ShapeInfo& info : shapeInfos) {
        QString displayText = QString("[%1] %2 (ID: %3) - точка привязки: (%4, %5)")
            .arg(info.type)
            .arg(info.name.isEmpty() ? "Без имени" : info.name)
            .arg(info.id)
            .arg(info.savedAnchorX)
            .arg(info.savedAnchorY);
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setCheckState(Qt::Checked);
        listWidget->addItem(item);
        itemToInfo[item] = info;
    }
    
    layout->addWidget(listWidget);
    
    QCheckBox* selectAllCheck = new QCheckBox("Выбрать все");
    selectAllCheck->setChecked(true);
    connect(selectAllCheck, &QCheckBox::stateChanged, [listWidget](int state) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setCheckState(state == Qt::Checked ? Qt::Checked : Qt::Unchecked);
        }
    });
    layout->addWidget(selectAllCheck);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() != QDialog::Accepted) return;
    
    int loadedCount = 0;
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item->checkState() == Qt::Checked && itemToInfo.contains(item)) {
            ShapeInfo info = itemToInfo[item];
            
            Shape* shape = nullptr;
            if (info.type == "Polygon") {
                shape = new Polygon();
            } else if (info.type == "Ellipse") {
                shape = new Ellipse();
            } else if (info.type == "Composite") {
                shape = new CompositeShape("");
            }
            
            if (shape && shape->fromJson(info.json)) {
                int dx = info.savedAnchorX - shape->getAnchorPoint().x();
                int dy = info.savedAnchorY - shape->getAnchorPoint().y();
                shape->move(dx, dy);
                
                if (shape->getId() >= m_nextId) {
                    m_nextId = shape->getId() + 1;
                }
                
                m_shapes.append(shape);
                addToShapeList(shape);
                loadedCount++;
            } else {
                delete shape;
            }
        }
    }
    
    if (loadedCount > 0) {
        if (m_isPropertiesVisible) {
            m_propertiesScrollArea->hide();
            m_isPropertiesVisible = false;
        }
        
        QMessageBox::information(this, "Успех", 
            QString("Загружено %1 из %2 фигур")
            .arg(loadedCount)
            .arg(shapeInfos.size()));
        
        if (!m_shapes.isEmpty()) {
            onShapeSelected(m_shapes.last());
        }
        
        m_needsRedraw = true;
        update();
    } else {
        QMessageBox::warning(this, "Предупреждение", "Не загружено ни одной фигуры");
    }
}
void MainWindow::saveSelectedShapesToFile()
{
    if (m_shapes.isEmpty()) {
        QMessageBox::information(this, "Внимание", "Нет фигур для сохранения");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Выберите фигуры для сохранения");
    dialog.setModal(true);
    dialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Выберите фигуры для сохранения:");
    layout->addWidget(label);
    
    QListWidget* listWidget = new QListWidget();
    listWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    
    QHash<QListWidgetItem*, Shape*> itemToShape;
    
    for (Shape* shape : m_shapes) {
        QString displayText = QString("[%1] %2 (ID: %3)")
            .arg(shape->getType())
            .arg(shape->getName())
            .arg(shape->getId());
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setCheckState(Qt::Checked);
        listWidget->addItem(item);
        itemToShape[item] = shape;
    }
    
    layout->addWidget(listWidget);
    
    QCheckBox* selectAllCheck = new QCheckBox("Выбрать все");
    selectAllCheck->setChecked(true);
    connect(selectAllCheck, &QCheckBox::stateChanged, [listWidget](int state) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setCheckState(state == Qt::Checked ? Qt::Checked : Qt::Unchecked);
        }
    });
    layout->addWidget(selectAllCheck);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() != QDialog::Accepted) return;
    
    QString filename = QFileDialog::getSaveFileName(this, 
        "Сохранить фигуры", 
        QDir::homePath() + "/shapes.json", 
        "JSON files (*.json)");
    
    if (filename.isEmpty()) return;
    
    QJsonArray shapesArray;
    int savedCount = 0;
    
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item->checkState() == Qt::Checked && itemToShape.contains(item)) {
            Shape* shape = itemToShape[item];
            QJsonObject json = shape->toJson();
            json["saved_anchor_x"] = shape->getAnchorPoint().x();
            json["saved_anchor_y"] = shape->getAnchorPoint().y();
            shapesArray.append(json);
            savedCount++;
        }
    }
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }
    
    QJsonDocument doc(shapesArray);
    file.write(doc.toJson());
    file.close();
    
    QMessageBox::information(this, "Успех", 
        QString("Сохранено %1 фигур в файл:\n%2")
        .arg(savedCount)
        .arg(filename));
}
void MainWindow::closeApplication()
{
    close();
    QApplication::quit();
}

void MainWindow::createShapeSelectionPanel()
{
    m_shapeSelectionPanel = new QWidget(this);
    m_shapeSelectionPanel->setGeometry(10, 10, 320, 900);
    m_shapeSelectionPanel->setStyleSheet("background-color: #f0f0f0; border: 1px solid gray;");
    
    QVBoxLayout* layout = new QVBoxLayout(m_shapeSelectionPanel);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);
    
    QLabel* title = new QLabel("СОЗДАНИЕ ФИГУР");
    title->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(title);
    
    QPushButton* polygonBtn = new QPushButton("Создать многоугольник (углы/стороны)");
    polygonBtn->setFixedHeight(30);
    connect(polygonBtn, &QPushButton::clicked, this, &MainWindow::createPolygonDialog);
    layout->addWidget(polygonBtn);
    
    QPushButton* ellipseBtn = new QPushButton("Создать эллипс/окружность");
    ellipseBtn->setFixedHeight(30);
    connect(ellipseBtn, &QPushButton::clicked, this, &MainWindow::createEllipseDialog);
    layout->addWidget(ellipseBtn);
    
    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet("background-color: gray;");
    layout->addWidget(line1);
    
    QLabel* presetTitle = new QLabel("ГОТОВЫЕ ФИГУРЫ");
    presetTitle->setStyleSheet("font-size: 12px; font-weight: bold;");
    layout->addWidget(presetTitle);
    
    QPushButton* triangleBtn = new QPushButton("Треугольник");
    triangleBtn->setFixedHeight(28);
    connect(triangleBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Треугольник");
        polygon->setId(getNextId());
        polygon->addVertex(QPoint(0, -60));
        polygon->addVertex(QPoint(-52, 30));
        polygon->addVertex(QPoint(52, 30));
        polygon->closePolygon();
        polygon->edgeColors = {Qt::darkRed, Qt::darkGreen, Qt::darkBlue};
        polygon->edgeWidths = {3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(255, 200, 200));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(triangleBtn);
    
    QPushButton* squareBtn = new QPushButton("Квадрат");
    squareBtn->setFixedHeight(28);
    connect(squareBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Квадрат");
        polygon->setId(getNextId());
        polygon->addVertex(QPoint(-50, -50));
        polygon->addVertex(QPoint(50, -50));
        polygon->addVertex(QPoint(50, 50));
        polygon->addVertex(QPoint(-50, 50));
        polygon->closePolygon();
        polygon->edgeColors = {QColor(100, 100, 200), QColor(100, 100, 200),
                               QColor(100, 100, 200), QColor(100, 100, 200)};
        polygon->edgeWidths = {3, 3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(200, 200, 255));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(squareBtn);
    
    QPushButton* rectangleBtn = new QPushButton("Прямоугольник");
    rectangleBtn->setFixedHeight(28);
    connect(rectangleBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Прямоугольник");
        polygon->setId(getNextId());
        polygon->addVertex(QPoint(-75, -40));
        polygon->addVertex(QPoint(75, -40));
        polygon->addVertex(QPoint(75, 40));
        polygon->addVertex(QPoint(-75, 40));
        polygon->closePolygon();
        polygon->edgeColors = {QColor(76, 175, 80), QColor(139, 195, 74),
                               QColor(76, 175, 80), QColor(139, 195, 74)};
        polygon->edgeWidths = {3, 3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(200, 255, 200));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(rectangleBtn);
    
    QPushButton* trapezoidBtn = new QPushButton("Трапеция");
    trapezoidBtn->setFixedHeight(28);
    connect(trapezoidBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Трапеция");
        polygon->setId(getNextId());
        polygon->addVertex(QPoint(-80, -40));
        polygon->addVertex(QPoint(80, -40));
        polygon->addVertex(QPoint(50, 40));
        polygon->addVertex(QPoint(-50, 40));
        polygon->closePolygon();
        polygon->edgeColors = {QColor(255, 152, 0), QColor(255, 193, 7),
                               QColor(255, 152, 0), QColor(255, 193, 7)};
        polygon->edgeWidths = {3, 3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(255, 235, 200));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(trapezoidBtn);
    
    QPushButton* pentagonBtn = new QPushButton("Пятиугольник");
    pentagonBtn->setFixedHeight(28);
    connect(pentagonBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Пятиугольник");
        polygon->setId(getNextId());
        double angle = -M_PI / 2;
        double radius = 60;
        for (int i = 0; i < 5; ++i) {
            double x = radius * cos(angle);
            double y = radius * sin(angle);
            polygon->addVertex(QPoint(static_cast<int>(x), static_cast<int>(y)));
            angle += 2 * M_PI / 5;
        }
        polygon->closePolygon();
        polygon->edgeColors = {QColor(156, 39, 176), QColor(156, 39, 176),
                               QColor(156, 39, 176), QColor(156, 39, 176),
                               QColor(156, 39, 176)};
        polygon->edgeWidths = {3, 3, 3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(225, 190, 231));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(pentagonBtn);
    
    QPushButton* hexagonBtn = new QPushButton("Шестиугольник");
    hexagonBtn->setFixedHeight(28);
    connect(hexagonBtn, &QPushButton::clicked, [this]() {
        Polygon* polygon = new Polygon("Шестиугольник");
        polygon->setId(getNextId());
        double angle = 0;
        double radius = 60;
        for (int i = 0; i < 6; ++i) {
            double x = radius * cos(angle);
            double y = radius * sin(angle);
            polygon->addVertex(QPoint(static_cast<int>(x), static_cast<int>(y)));
            angle += 2 * M_PI / 6;
        }
        polygon->closePolygon();
        polygon->edgeColors = {QColor(0, 150, 136), QColor(0, 150, 136),
                               QColor(0, 150, 136), QColor(0, 150, 136),
                               QColor(0, 150, 136), QColor(0, 150, 136)};
        polygon->edgeWidths = {3, 3, 3, 3, 3, 3};
        polygon->setFill(true);
        polygon->setFillColor(QColor(178, 223, 219));
        
        QPoint center = getCenterPosition();
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        polygon->move(center.x() - shapeCenter.x(), center.y() - shapeCenter.y());
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
    });
    layout->addWidget(hexagonBtn);
    
    QPushButton* circleBtn = new QPushButton("Окружность");
    circleBtn->setFixedHeight(28);
    connect(circleBtn, &QPushButton::clicked, [this]() {
        Ellipse* ellipse = new Ellipse("Окружность");
        ellipse->setId(getNextId());
        ellipse->setAsCircle(75);
        ellipse->setCenter(getCenterPosition());
        ellipse->setFill(true);
        ellipse->setFillColor(QColor(173, 216, 230));
        
        m_shapes.append(ellipse);
        addToShapeList(ellipse);
        onShapeSelected(ellipse);
    });
    layout->addWidget(circleBtn);
    
    QFrame* line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet("background-color: gray;");
    layout->addWidget(line2);
    
    QPushButton* saveSelectedBtn = new QPushButton("Сохранить выбранные фигуры");
    saveSelectedBtn->setFixedHeight(30);
    connect(saveSelectedBtn, &QPushButton::clicked, this, &MainWindow::saveSelectedShapesToFile);
    layout->addWidget(saveSelectedBtn);
    
    QPushButton* loadBtn = new QPushButton("Загрузить фигуры");
    loadBtn->setFixedHeight(30);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadShapesFromFile);
    layout->addWidget(loadBtn);
    
    QPushButton* clearBtn = new QPushButton("Очистить экран");
    clearBtn->setFixedHeight(30);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearAll);
    layout->addWidget(clearBtn);
    
    QPushButton* loadPluginBtn = new QPushButton("модуль");
    loadPluginBtn->setFixedHeight(30);
    connect(loadPluginBtn, &QPushButton::clicked, this, &MainWindow::loadExternalPlugin);
    layout->addWidget(loadPluginBtn);
    
    QFrame* lineComposite = new QFrame();
    lineComposite->setFrameShape(QFrame::HLine);
    lineComposite->setStyleSheet("background-color: gray;");
    layout->addWidget(lineComposite);
    
    QPushButton* startCompositeBtn = new QPushButton("Начать создание составной");
    startCompositeBtn->setFixedHeight(30);
    startCompositeBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(startCompositeBtn, &QPushButton::clicked, this, &MainWindow::startCompositeCreation);
    layout->addWidget(startCompositeBtn);
    
    QPushButton* ungroupBtn = new QPushButton("Разъединить составную фигуру");
    ungroupBtn->setFixedHeight(30);
    ungroupBtn->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold;");
    connect(ungroupBtn, &QPushButton::clicked, this, &MainWindow::ungroupCompositeShape);
    layout->addWidget(ungroupBtn);
    
    QHBoxLayout* compositeControlsLayout = new QHBoxLayout();
    compositeControlsLayout->setSpacing(4);
    
    m_cancelCompositeBtn = new QPushButton("Отмена");
    m_cancelCompositeBtn->setFixedHeight(28);
    m_cancelCompositeBtn->hide();
    connect(m_cancelCompositeBtn, &QPushButton::clicked, this, &MainWindow::cancelCompositeCreation);
    
    m_finishCompositeBtn = new QPushButton("Готово");
    m_finishCompositeBtn->setFixedHeight(28);
    m_finishCompositeBtn->hide();
    connect(m_finishCompositeBtn, &QPushButton::clicked, this, &MainWindow::finishCompositeCreation);
    
    compositeControlsLayout->addWidget(m_cancelCompositeBtn);
    compositeControlsLayout->addWidget(m_finishCompositeBtn);
    layout->addLayout(compositeControlsLayout);
    
    QFrame* line3 = new QFrame();
    line3->setFrameShape(QFrame::HLine);
    line3->setStyleSheet("background-color: gray;");
    layout->addWidget(line3);
    
    QLabel* shapesTitle = new QLabel("МОИ ФИГУРЫ");
    shapesTitle->setStyleSheet("font-size: 12px; font-weight: bold;");
    layout->addWidget(shapesTitle);
    
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(300);
    
    m_shapesContainer = new QWidget();
    QVBoxLayout* userLayout = new QVBoxLayout(m_shapesContainer);
    userLayout->setSpacing(2);
    userLayout->setContentsMargins(2, 2, 2, 2);
    userLayout->addStretch();
    
    scrollArea->setWidget(m_shapesContainer);
    layout->addWidget(scrollArea);
    
    createCloseButton();
}
void MainWindow::ungroupCompositeShape()
{
    if (!m_currentShape) {
        QMessageBox::information(this, "Внимание", "Нет выбранной фигуры");
        return;
    }
    
    CompositeShape* composite = dynamic_cast<CompositeShape*>(m_currentShape);
    if (!composite) {
        QMessageBox::information(this, "Внимание", "Выбранная фигура не является составной");
        return;
    }
    
    int result = QMessageBox::question(this, "Разъединение", 
        QString("Разъединить составную фигуру '%1' на отдельные фигуры?")
        .arg(composite->getName()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result != QMessageBox::Yes) return;
    
    const QList<Shape*>& subShapes = composite->getShapes();
    
    if (subShapes.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Составная фигура не содержит частей");
        return;
    }
    
    QList<Shape*> copies;
    
    for (Shape* subShape : subShapes) {
        Shape* copy = copyShapeRecursive(subShape);
        if (copy) {
            copies.append(copy);
        }
    }
    
    int index = m_shapes.indexOf(composite);
    if (index != -1) {
        m_shapes.removeAt(index);
    }
    
    delete composite;
    
    for (Shape* copy : copies) {
        m_shapes.append(copy);
        addToShapeList(copy);
    }
    
    m_currentShape = nullptr;
    
    if (m_isPropertiesVisible) {
        m_propertiesScrollArea->hide();
        m_isPropertiesVisible = false;
    }
    
    updateShapesList();
    
    if (!m_shapes.isEmpty()) {
        onShapeSelected(m_shapes.last());
    }
    
    QMessageBox::information(this, "Успех", 
        QString("Разъединено на %1 фигур").arg(copies.size()));
    
    m_needsRedraw = true;
    update();
}
void MainWindow::addToShapeList(Shape* shape)
{
    if (!m_shapesContainer || !shape) return;
    
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_shapesContainer->layout());
    if (!layout) return;
    
    QString displayText;
    if (dynamic_cast<CompositeShape*>(shape)) {
        displayText = QString("[СОСТАВНАЯ] %1 [ID:%2]").arg(shape->getName()).arg(shape->getId());
    } else {
        displayText = QString("%1 [ID:%2]").arg(shape->getName()).arg(shape->getId());
    }
    
    QPushButton* btn = new QPushButton(displayText);
    btn->setFixedHeight(30);
    btn->setToolTip(QString("ID: %1\nТип: %2").arg(shape->getId()).arg(shape->getType()));
    
    CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
    if (composite) {
        QMenu* menu = new QMenu(btn);
        menu->setTitle("Составные части");
        
        const QList<Shape*>& subShapes = composite->getShapes();
        for (Shape* subShape : subShapes) {
            if (subShape) {
                QString subText = QString("%1 [ID:%2]")
                    .arg(subShape->getName())
                    .arg(subShape->getId());
                QAction* action = menu->addAction(subText);
                action->setEnabled(false);
            }
        }
        
        if (subShapes.isEmpty()) {
            menu->addAction("(нет составных частей)")->setEnabled(false);
        }
        
        btn->setMenu(menu);
        btn->setStyleSheet("QPushButton::menu-indicator { image: none; }");
    }
    
    connect(btn, &QPushButton::clicked, [this, shape]() {
        if (shape) {
            onShapeSelected(shape);
        }
    });
    
    layout->insertWidget(layout->count() - 1, btn);
}

void MainWindow::updateShapesList()
{
    if (!m_shapesContainer) return;
    
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_shapesContainer->layout());
    if (!layout) return;
    
    while (layout->count() > 1) {
        QLayoutItem* item = layout->takeAt(0);
        if (item && item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    if (m_shapes.isEmpty()) {
        return;
    }
    
    for (Shape* shape : m_shapes) {
        if (!shape) continue;
        
        QString displayText;
        if (dynamic_cast<CompositeShape*>(shape)) {
            displayText = QString("[СОСТАВНАЯ] %1 [ID:%2]").arg(shape->getName()).arg(shape->getId());
        } else {
            displayText = QString("%1 [ID:%2]").arg(shape->getName()).arg(shape->getId());
        }
        
        QPushButton* btn = new QPushButton(displayText);
        btn->setFixedHeight(30);
        btn->setToolTip(QString("ID: %1\nТип: %2").arg(shape->getId()).arg(shape->getType()));
        
        CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
        if (composite) {
            QMenu* menu = new QMenu(btn);
            menu->setTitle("Составные части");
            
            const QList<Shape*>& subShapes = composite->getShapes();
            for (Shape* subShape : subShapes) {
                if (subShape) {
                    QString subText = QString("%1 [ID:%2]")
                        .arg(subShape->getName())
                        .arg(subShape->getId());
                    QAction* action = menu->addAction(subText);
                    action->setEnabled(false);
                }
            }
            
            if (subShapes.isEmpty()) {
                menu->addAction("(нет составных частей)")->setEnabled(false);
            }
            
            btn->setMenu(menu);
            btn->setStyleSheet("QPushButton::menu-indicator { image: none; }");
        }
        
        connect(btn, &QPushButton::clicked, [this, shape]() {
            if (shape) {
                onShapeSelected(shape);
            }
        });
        
        layout->insertWidget(layout->count() - 1, btn);
    }
}
void MainWindow::onShapeSelected(Shape* shape)
{
    if (!shape) return;
    
    if (m_currentShape) {
        m_currentShape->setSelected(false);
    }
    
    m_currentShape = shape;
    m_currentShape->setSelected(true);
    
    if (!m_isPropertiesVisible) {
        m_propertiesScrollArea->show();
        m_isPropertiesVisible = true;
    }
    
    updatePropertiesPanel();
    m_needsRedraw = true;
    update();
}

void MainWindow::updatePropertiesPanel()
{
    if (!m_currentShape || !m_propertiesPanel) return;
    
    QLabel* idLabel = m_propertiesPanel->findChild<QLabel*>("idLabel");
    if (idLabel) {
        idLabel->setText(QString("ID: %1").arg(m_currentShape->getId()));
    }
    
    QLineEdit* nameEdit = m_propertiesPanel->findChild<QLineEdit*>("nameEdit");
    if (nameEdit) {
        nameEdit->setText(m_currentShape->getName());
    }
    
    QLabel* coordsLabel = m_propertiesPanel->findChild<QLabel*>("coordsLabel");
    if (coordsLabel) {
        coordsLabel->setText(QString("Точка привязки: X=%1 Y=%2")
            .arg(m_currentShape->getAnchorPoint().x())
            .arg(m_currentShape->getAnchorPoint().y()));
    }
    
    QSpinBox* spinX = m_propertiesPanel->findChild<QSpinBox*>("spinX");
    QSpinBox* spinY = m_propertiesPanel->findChild<QSpinBox*>("spinY");
    if (spinX && spinY) {
        spinX->setValue(m_currentShape->getAnchorPoint().x());
        spinY->setValue(m_currentShape->getAnchorPoint().y());
    }
    
    QCheckBox* fillCheck = m_propertiesPanel->findChild<QCheckBox*>("fillCheck");
    QPushButton* fillColorBtn = m_propertiesPanel->findChild<QPushButton*>("fillColorBtn");
    
    if (fillCheck) {
        fillCheck->setChecked(m_currentShape->hasFill());
        disconnect(fillCheck, &QCheckBox::toggled, nullptr, nullptr);
        connect(fillCheck, &QCheckBox::toggled, [this](bool checked) {
            if (m_currentShape) {
                m_currentShape->setFill(checked);
                m_needsRedraw = true;
            }
        });
    }
    
    if (fillColorBtn) {
        fillColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(m_currentShape->getFillColor().name()));
        disconnect(fillColorBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(fillColorBtn, &QPushButton::clicked, [this, fillColorBtn]() {
            if (m_currentShape) {
                QColor color = QColorDialog::getColor(m_currentShape->getFillColor(), this);
                if (color.isValid()) {
                    m_currentShape->setFillColor(color);
                    fillColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
                    m_needsRedraw = true;
                }
            }
        });
    }
    
    updateSideWidgets();
    updateVertexCoordinates();
}

void MainWindow::updateVertexCoordinates()
{
    if (!m_currentShape) return;
    
    if (!m_vertexTable) {
        m_vertexTable = new QTableWidget(m_propertiesPanel);
        m_vertexTable->setColumnCount(3);
        m_vertexTable->setHorizontalHeaderLabels({"Вершина/Центр", "X (отн.)", "Y (отн.)"});
        m_vertexTable->horizontalHeader()->setStretchLastSection(true);
        m_vertexTable->setMaximumHeight(150);
        
        QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(m_propertiesPanel->layout());
        if (mainLayout) {
            int index = mainLayout->count() - 1;
            mainLayout->insertWidget(index, m_vertexTable);
        }
    }
    
    m_vertexTable->setRowCount(m_currentShape->relativePoints.size());
    
    for (int i = 0; i < m_currentShape->relativePoints.size(); ++i) {
        const QPoint& relPoint = m_currentShape->relativePoints[i];
        
        QString vertexName;
        if (i == 0) {
            vertexName = "Центр";
        } else {
            vertexName = QString("Радиус %1").arg(i);
        }
        
        QTableWidgetItem* vertexItem = new QTableWidgetItem(vertexName);
        vertexItem->setTextAlignment(Qt::AlignCenter);
        vertexItem->setFlags(vertexItem->flags() & ~Qt::ItemIsEditable);
        m_vertexTable->setItem(i, 0, vertexItem);
        
        QTableWidgetItem* xItem = new QTableWidgetItem(QString::number(relPoint.x()));
        xItem->setTextAlignment(Qt::AlignCenter);
        xItem->setFlags(xItem->flags() & ~Qt::ItemIsEditable);
        m_vertexTable->setItem(i, 1, xItem);
        
        QTableWidgetItem* yItem = new QTableWidgetItem(QString::number(relPoint.y()));
        yItem->setTextAlignment(Qt::AlignCenter);
        yItem->setFlags(yItem->flags() & ~Qt::ItemIsEditable);
        m_vertexTable->setItem(i, 2, yItem);
    }
    
    m_vertexTable->resizeColumnsToContents();
}

void MainWindow::updateSideWidgets()
{
    if (!m_currentShape) return;
    
    while (m_sideTabs->count() > 0) {
        m_sideTabs->removeTab(0);
    }
    m_sidePages.clear();
    
    Polygon* polygon = dynamic_cast<Polygon*>(m_currentShape);
    Ellipse* ellipse = dynamic_cast<Ellipse*>(m_currentShape);
    CompositeShape* composite = dynamic_cast<CompositeShape*>(m_currentShape);
    
    if (polygon && polygon->isClosed()) {
        m_sideTabs->setVisible(true);
        
        for (int i = 0; i < polygon->vertexCount(); ++i) {
            QWidget* page = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(page);
            layout->setSpacing(8);
            layout->setContentsMargins(8, 8, 8, 8);
            
            QLabel* titleLabel = new QLabel(QString("Сторона %1").arg(i + 1));
            titleLabel->setStyleSheet("font-size: 12px; font-weight: bold;");
            layout->addWidget(titleLabel);
            
            QFrame* line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            layout->addWidget(line);
            
            QHBoxLayout* angleLayout = new QHBoxLayout();
            angleLayout->addWidget(new QLabel("Угол:"));
            QSpinBox* angleSpin = new QSpinBox();
            angleSpin->setRange(1, 179);
            angleSpin->setValue(polygon->getSideAngle(i));
            angleSpin->setSuffix("°");
            
            connect(angleSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                    [this, polygon, i](int value) {
                polygon->setSideAngle(i, value);
                m_needsRedraw = true;
                updatePropertiesPanel();
            });
            
            angleLayout->addWidget(angleSpin);
            angleLayout->addStretch();
            layout->addLayout(angleLayout);
            
            QHBoxLayout* lengthLayout = new QHBoxLayout();
            lengthLayout->addWidget(new QLabel("Длина:"));
            QSpinBox* lengthSpin = new QSpinBox();
            lengthSpin->setRange(10, 10000);
            lengthSpin->setValue(polygon->getSideLength(i));
            lengthSpin->setSuffix("px");
            
            connect(lengthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                    [this, polygon, i](int value) {
                polygon->setSideLength(i, value);
                m_needsRedraw = true;
                updatePropertiesPanel();
            });
            
            lengthLayout->addWidget(lengthSpin);
            lengthLayout->addStretch();
            layout->addLayout(lengthLayout);
            
            QHBoxLayout* colorLayout = new QHBoxLayout();
            colorLayout->addWidget(new QLabel("Цвет:"));
            QPushButton* colorBtn = new QPushButton();
            colorBtn->setFixedSize(60, 25);
            if (i < polygon->edgeColors.size()) {
                colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(polygon->edgeColors[i].name()));
            }
            
            connect(colorBtn, &QPushButton::clicked, [this, polygon, i, colorBtn]() {
                QColor color = QColorDialog::getColor(polygon->edgeColors[i], this);
                if (color.isValid()) {
                    polygon->setSideColor(i, color);
                    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
                    m_needsRedraw = true;
                }
            });
            
            colorLayout->addWidget(colorBtn);
            colorLayout->addStretch();
            layout->addLayout(colorLayout);
            
            QHBoxLayout* widthLayout = new QHBoxLayout();
            widthLayout->addWidget(new QLabel("Толщина:"));
            QSpinBox* widthSpin = new QSpinBox();
            widthSpin->setRange(1, 9999);
            if (i < polygon->edgeWidths.size()) {
                widthSpin->setValue(polygon->edgeWidths[i]);
            }
            
            connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                    [this, polygon, i](int value) {
                polygon->setSideWidth(i, value);
                m_needsRedraw = true;
                updatePropertiesPanel();
            });
            
            widthLayout->addWidget(widthSpin);
            widthLayout->addStretch();
            layout->addLayout(widthLayout);
            
            layout->addStretch();
            m_sideTabs->addTab(page, QString::number(i + 1));
            m_sidePages.append(page);
        }
    }
    else if (ellipse) {
        m_sideTabs->setVisible(true);
        QWidget* page = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setSpacing(8);
        layout->setContentsMargins(8, 8, 8, 8);
        
        QLabel* titleLabel = new QLabel("Параметры эллипса");
        titleLabel->setStyleSheet("font-size: 12px; font-weight: bold;");
        layout->addWidget(titleLabel);
        
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        layout->addWidget(line);
        
        QHBoxLayout* rxLayout = new QHBoxLayout();
        rxLayout->addWidget(new QLabel("Радиус X:"));
        QSpinBox* rxSpin = new QSpinBox();
        rxSpin->setRange(10, 10000);
        rxSpin->setValue(ellipse->getRadiusX());
        
        connect(rxSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse](int value) {
            ellipse->setRadiusX(value);
            m_needsRedraw = true;
            updatePropertiesPanel();
        });
        
        rxLayout->addWidget(rxSpin);
        rxLayout->addStretch();
        layout->addLayout(rxLayout);
        
        QHBoxLayout* ryLayout = new QHBoxLayout();
        ryLayout->addWidget(new QLabel("Радиус Y:"));
        QSpinBox* rySpin = new QSpinBox();
        rySpin->setRange(10, 10000);
        rySpin->setValue(ellipse->getRadiusY());
        
        connect(rySpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse](int value) {
            ellipse->setRadiusY(value);
            m_needsRedraw = true;
            updatePropertiesPanel();
        });
        
        ryLayout->addWidget(rySpin);
        ryLayout->addStretch();
        layout->addLayout(ryLayout);
        
        QHBoxLayout* angleLayout = new QHBoxLayout();
        angleLayout->addWidget(new QLabel("Угол поворота:"));
        QSpinBox* angleSpin = new QSpinBox();
        angleSpin->setRange(0, 359);
        angleSpin->setValue(ellipse->getAngle());
        angleSpin->setSuffix("°");
        
        connect(angleSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse](int value) {
            ellipse->setAngle(value);
            m_needsRedraw = true;
            updatePropertiesPanel();
        });
        
        angleLayout->addWidget(angleSpin);
        angleLayout->addStretch();
        layout->addLayout(angleLayout);
        
        QGroupBox* focusGroup = new QGroupBox("Фокусы");
QVBoxLayout* focusLayout = new QVBoxLayout(focusGroup);

QHBoxLayout* focus1Layout = new QHBoxLayout();
focus1Layout->addWidget(new QLabel("Фокус 1:"));
QSpinBox* focus1X = new QSpinBox();
focus1X->setRange(0, 5000);
focus1X->setValue(ellipse->getFocus1().x());
QSpinBox* focus1Y = new QSpinBox();
focus1Y->setRange(0, 5000);
focus1Y->setValue(ellipse->getFocus1().y());

connect(focus1X, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse, focus1Y](int x) {
    QPoint newPos(x, focus1Y->value());
    ellipse->setFocus1(newPos);
    m_needsRedraw = true;
    update();
});

connect(focus1Y, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse, focus1X](int y) {
    QPoint newPos(focus1X->value(), y);
    ellipse->setFocus1(newPos);
    m_needsRedraw = true;
    update();
});

focus1Layout->addWidget(focus1X);
focus1Layout->addWidget(new QLabel(","));
focus1Layout->addWidget(focus1Y);
focus1Layout->addStretch();
focusLayout->addLayout(focus1Layout);

QHBoxLayout* focus2Layout = new QHBoxLayout();
focus2Layout->addWidget(new QLabel("Фокус 2:"));
QSpinBox* focus2X = new QSpinBox();
focus2X->setRange(0, 5000);
focus2X->setValue(ellipse->getFocus2().x());
QSpinBox* focus2Y = new QSpinBox();
focus2Y->setRange(0, 5000);
focus2Y->setValue(ellipse->getFocus2().y());

connect(focus2X, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse, focus2Y](int x) {
    QPoint newPos(x, focus2Y->value());
    ellipse->setFocus2(newPos);
    m_needsRedraw = true;
    update();
});

connect(focus2Y, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse, focus2X](int y) {
    QPoint newPos(focus2X->value(), y);
    ellipse->setFocus2(newPos);
    m_needsRedraw = true;
    update();
});

focus2Layout->addWidget(focus2X);
focus2Layout->addWidget(new QLabel(","));
focus2Layout->addWidget(focus2Y);
focus2Layout->addStretch();
focusLayout->addLayout(focus2Layout);

layout->addWidget(focusGroup);

        
        QHBoxLayout* colorLayout = new QHBoxLayout();
        colorLayout->addWidget(new QLabel("Цвет линии:"));
        QPushButton* colorBtn = new QPushButton();
        colorBtn->setFixedSize(60, 25);
        if (!ellipse->edgeColors.isEmpty()) {
            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(ellipse->edgeColors[0].name()));
        }
        
        connect(colorBtn, &QPushButton::clicked, [this, ellipse, colorBtn]() {
            QColor color = QColorDialog::getColor(ellipse->edgeColors[0], this);
            if (color.isValid()) {
                ellipse->edgeColors[0] = color;
                colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
                m_needsRedraw = true;
            }
        });
        
        colorLayout->addWidget(colorBtn);
        colorLayout->addStretch();
        layout->addLayout(colorLayout);
        
        QHBoxLayout* widthLayout = new QHBoxLayout();
        widthLayout->addWidget(new QLabel("Толщина:"));
        QSpinBox* widthSpin = new QSpinBox();
        widthSpin->setRange(1, 9999);
        if (!ellipse->edgeWidths.isEmpty()) {
            widthSpin->setValue(ellipse->edgeWidths[0]);
        }
        
        connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, ellipse](int value) {
            ellipse->edgeWidths[0] = value;
            m_needsRedraw = true;
            updatePropertiesPanel();
        });
        
        widthLayout->addWidget(widthSpin);
        widthLayout->addStretch();
        layout->addLayout(widthLayout);
        
        layout->addStretch();
        m_sideTabs->addTab(page, "1");
        m_sidePages.append(page);
    }
    else if (composite) {
        const QList<Shape*>& subShapes = composite->getShapes();
        
        if (subShapes.isEmpty()) {
            m_sideTabs->setVisible(false);
            return;
        }
        
        m_sideTabs->setVisible(true);
        
        int shapeIndex = 0;
        for (Shape* subShape : subShapes) {
            Polygon* subPolygon = dynamic_cast<Polygon*>(subShape);
            Ellipse* subEllipse = dynamic_cast<Ellipse*>(subShape);
            
            QString shapeName = subShape->getName();
            if (shapeName.isEmpty()) {
                shapeName = subShape->getType();
            }
            
            if (subPolygon && subPolygon->isClosed()) {
                for (int i = 0; i < subPolygon->vertexCount(); ++i) {
                    QWidget* page = new QWidget();
                    QVBoxLayout* layout = new QVBoxLayout(page);
                    layout->setSpacing(8);
                    layout->setContentsMargins(8, 8, 8, 8);
                    
                    QLabel* titleLabel = new QLabel(QString("%1 - Сторона %2").arg(shapeName).arg(i + 1));
                    titleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #4CAF50;");
                    layout->addWidget(titleLabel);
                    
                    QFrame* line = new QFrame();
                    line->setFrameShape(QFrame::HLine);
                    layout->addWidget(line);
                    
                    QHBoxLayout* angleLayout = new QHBoxLayout();
                    angleLayout->addWidget(new QLabel("Угол:"));
                    QSpinBox* angleSpin = new QSpinBox();
                    angleSpin->setRange(1, 179);
                    angleSpin->setValue(subPolygon->getSideAngle(i));
                    angleSpin->setSuffix("°");
                    
                    connect(angleSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                            [this, subPolygon, i](int value) {
                        subPolygon->setSideAngle(i, value);
                        m_needsRedraw = true;
                        update();
                    });
                    
                    angleLayout->addWidget(angleSpin);
                    angleLayout->addStretch();
                    layout->addLayout(angleLayout);
                    
                    QHBoxLayout* lengthLayout = new QHBoxLayout();
                    lengthLayout->addWidget(new QLabel("Длина:"));
                    QSpinBox* lengthSpin = new QSpinBox();
                    lengthSpin->setRange(10, 10000);
                    lengthSpin->setValue(subPolygon->getSideLength(i));
                    lengthSpin->setSuffix("px");
                    
                    connect(lengthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                            [this, subPolygon, i](int value) {
                        subPolygon->setSideLength(i, value);
                        m_needsRedraw = true;
                        update();
                    });
                    
                    lengthLayout->addWidget(lengthSpin);
                    lengthLayout->addStretch();
                    layout->addLayout(lengthLayout);
                    
                    QHBoxLayout* colorLayout = new QHBoxLayout();
                    colorLayout->addWidget(new QLabel("Цвет:"));
                    QPushButton* colorBtn = new QPushButton();
                    colorBtn->setFixedSize(60, 25);
                    if (i < subPolygon->edgeColors.size()) {
                        colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(subPolygon->edgeColors[i].name()));
                    }
                    
                    connect(colorBtn, &QPushButton::clicked, [this, subPolygon, i, colorBtn]() {
                        QColor color = QColorDialog::getColor(subPolygon->edgeColors[i], this);
                        if (color.isValid()) {
                            subPolygon->setSideColor(i, color);
                            colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
                            m_needsRedraw = true;
                        }
                    });
                    
                    colorLayout->addWidget(colorBtn);
                    colorLayout->addStretch();
                    layout->addLayout(colorLayout);
                    
                    QHBoxLayout* widthLayout = new QHBoxLayout();
                    widthLayout->addWidget(new QLabel("Толщина:"));
                    QSpinBox* widthSpin = new QSpinBox();
                    widthSpin->setRange(1, 9999);
                    if (i < subPolygon->edgeWidths.size()) {
                        widthSpin->setValue(subPolygon->edgeWidths[i]);
                    }
                    
                    connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                            [this, subPolygon, i](int value) {
                        subPolygon->setSideWidth(i, value);
                        m_needsRedraw = true;
                        update();
                    });
                    
                    widthLayout->addWidget(widthSpin);
                    widthLayout->addStretch();
                    layout->addLayout(widthLayout);
                    
                    layout->addStretch();
                    m_sideTabs->addTab(page, QString("%1:%2").arg(shapeIndex + 1).arg(i + 1));
                    m_sidePages.append(page);
                }
            }
            else if (subEllipse) {
                QWidget* page = new QWidget();
                QVBoxLayout* layout = new QVBoxLayout(page);
                layout->setSpacing(8);
                layout->setContentsMargins(8, 8, 8, 8);
                
                QLabel* titleLabel = new QLabel(shapeName);
                titleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #4CAF50;");
                layout->addWidget(titleLabel);
                
                QFrame* line = new QFrame();
                line->setFrameShape(QFrame::HLine);
                layout->addWidget(line);
                
                QHBoxLayout* rxLayout = new QHBoxLayout();
                rxLayout->addWidget(new QLabel("Радиус X:"));
                QSpinBox* rxSpin = new QSpinBox();
                rxSpin->setRange(10, 10000);
                rxSpin->setValue(subEllipse->getRadiusX());
                
                connect(rxSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, subEllipse](int value) {
                    subEllipse->setRadiusX(value);
                    m_needsRedraw = true;
                    update();
                });
                
                rxLayout->addWidget(rxSpin);
                rxLayout->addStretch();
                layout->addLayout(rxLayout);
                
                QHBoxLayout* ryLayout = new QHBoxLayout();
                ryLayout->addWidget(new QLabel("Радиус Y:"));
                QSpinBox* rySpin = new QSpinBox();
                rySpin->setRange(10, 10000);
                rySpin->setValue(subEllipse->getRadiusY());
                
                connect(rySpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, subEllipse](int value) {
                    subEllipse->setRadiusY(value);
                    m_needsRedraw = true;
                    update();
                });
                
                ryLayout->addWidget(rySpin);
                ryLayout->addStretch();
                layout->addLayout(ryLayout);
                
                QHBoxLayout* angleLayout = new QHBoxLayout();
                angleLayout->addWidget(new QLabel("Угол поворота:"));
                QSpinBox* angleSpin = new QSpinBox();
                angleSpin->setRange(0, 359);
                angleSpin->setValue(subEllipse->getAngle());
                angleSpin->setSuffix("°");
                
                connect(angleSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, subEllipse](int value) {
                    subEllipse->setAngle(value);
                    m_needsRedraw = true;
                    update();
                });
                
                angleLayout->addWidget(angleSpin);
                angleLayout->addStretch();
                layout->addLayout(angleLayout);
                
                QHBoxLayout* colorLayout = new QHBoxLayout();
                colorLayout->addWidget(new QLabel("Цвет линии:"));
                QPushButton* colorBtn = new QPushButton();
                colorBtn->setFixedSize(60, 25);
                if (!subEllipse->edgeColors.isEmpty()) {
                    colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(subEllipse->edgeColors[0].name()));
                }
                
                connect(colorBtn, &QPushButton::clicked, [this, subEllipse, colorBtn]() {
                    QColor color = QColorDialog::getColor(subEllipse->edgeColors[0], this);
                    if (color.isValid()) {
                        subEllipse->edgeColors[0] = color;
                        colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));
                        m_needsRedraw = true;
                    }
                });
                
                colorLayout->addWidget(colorBtn);
                colorLayout->addStretch();
                layout->addLayout(colorLayout);
                
                QHBoxLayout* widthLayout = new QHBoxLayout();
                widthLayout->addWidget(new QLabel("Толщина:"));
                QSpinBox* widthSpin = new QSpinBox();
                widthSpin->setRange(1, 9999);
                if (!subEllipse->edgeWidths.isEmpty()) {
                    widthSpin->setValue(subEllipse->edgeWidths[0]);
                }
                
                connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, subEllipse](int value) {
                    subEllipse->edgeWidths[0] = value;
                    m_needsRedraw = true;
                    update();
                });
                
                widthLayout->addWidget(widthSpin);
                widthLayout->addStretch();
                layout->addLayout(widthLayout);
                
                layout->addStretch();
                m_sideTabs->addTab(page, QString::number(shapeIndex + 1));
                m_sidePages.append(page);
            }
            
            shapeIndex++;
        }
    }
    else {
        m_sideTabs->setVisible(false);
    }
}
QPoint MainWindow::getCenterPosition() const
{
    return QPoint(width() / 2, height() / 2);
}

void MainWindow::hidePropertiesPanel()
{
    m_propertiesScrollArea->hide();
    m_isPropertiesVisible = false;
    m_needsRedraw = true;
    update();
}

void MainWindow::createPolygonDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Создание многоугольника");
    dialog.setModal(true);
    dialog.resize(500, 550);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* titleLabel = new QLabel("СОЗДАНИЕ МНОГОУГОЛЬНИКА");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    QGroupBox* paramsGroup = new QGroupBox("Параметры стороны");
    QGridLayout* paramsLayout = new QGridLayout(paramsGroup);
    
    paramsLayout->addWidget(new QLabel("Угол:"), 0, 0);
    QComboBox* angleCombo = new QComboBox();
    angleCombo->addItems({"30°", "45°", "60°", "90°", "120°", "150°", "Другой"});
    paramsLayout->addWidget(angleCombo, 0, 1);
    
    QSpinBox* customAngleSpin = new QSpinBox();
    customAngleSpin->setRange(1, 179);
    customAngleSpin->setValue(90);
    customAngleSpin->setEnabled(false);
    customAngleSpin->setSuffix("°");
    paramsLayout->addWidget(customAngleSpin, 0, 2);
    
    connect(angleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [customAngleSpin](int index) {
        customAngleSpin->setEnabled(index == 6);
    });
    
    paramsLayout->addWidget(new QLabel("Длина:"), 1, 0);
    QSpinBox* lengthSpin = new QSpinBox();
    lengthSpin->setRange(20, 5000);
    lengthSpin->setValue(100);
    lengthSpin->setSuffix(" пикс");
    paramsLayout->addWidget(lengthSpin, 1, 1, 1, 2);
    
    paramsLayout->addWidget(new QLabel("Цвет:"), 2, 0);
    QPushButton* colorBtn = new QPushButton();
    colorBtn->setFixedSize(50, 25);
    colorBtn->setStyleSheet("background-color: black;");
    QColor currentColor = Qt::black;
    
    connect(colorBtn, &QPushButton::clicked, [&currentColor, colorBtn, this]() {
        QColor color = QColorDialog::getColor(currentColor, this);
        if (color.isValid()) {
            currentColor = color;
            colorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        }
    });
    
    paramsLayout->addWidget(colorBtn, 2, 1);
    
    paramsLayout->addWidget(new QLabel("Толщина:"), 3, 0);
    QSpinBox* widthSpin = new QSpinBox();
    widthSpin->setRange(1, 9999);
    widthSpin->setValue(3);
    paramsLayout->addWidget(widthSpin, 3, 1, 1, 2);
    
    layout->addWidget(paramsGroup);
    
    QLabel* listLabel = new QLabel("Добавленные стороны:");
    listLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(listLabel);
    
    QListWidget* segmentsList = new QListWidget();
    segmentsList->setMaximumHeight(150);
    layout->addWidget(segmentsList);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("Добавить сторону");
    addBtn->setFixedHeight(30);
    QPushButton* removeBtn = new QPushButton("Удалить последнюю");
    removeBtn->setFixedHeight(30);
    
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    layout->addLayout(btnLayout);
    
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Название:"));
    QLineEdit* nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("Введите название");
    nameLayout->addWidget(nameEdit);
    layout->addLayout(nameLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    QList<int> tempAngles;
    QList<int> tempLengths;
    QList<QColor> tempColors;
    QList<int> tempWidths;
    
    auto updateList = [&]() {
        segmentsList->clear();
        for (int i = 0; i < tempAngles.size(); ++i) {
            segmentsList->addItem(QString("Сторона %1: угол=%2°, длина=%3")
                .arg(i + 1)
                .arg(tempAngles[i])
                .arg(tempLengths[i]));
        }
    };
    
    connect(addBtn, &QPushButton::clicked, [&]() {
        int angle;
        if (angleCombo->currentIndex() == 6) {
            angle = customAngleSpin->value();
        } else {
            QString angleText = angleCombo->currentText();
            angleText.replace("°", "");
            angle = angleText.toInt();
        }
        
        tempAngles.append(angle);
        tempLengths.append(lengthSpin->value());
        tempColors.append(currentColor);
        tempWidths.append(widthSpin->value());
        updateList();
    });
    
    connect(removeBtn, &QPushButton::clicked, [&]() {
        if (!tempAngles.isEmpty()) {
            tempAngles.removeLast();
            tempLengths.removeLast();
            tempColors.removeLast();
            tempWidths.removeLast();
            updateList();
        }
    });
    
    connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        if (tempAngles.size() < 3) {
            QMessageBox::warning(&dialog, "Ошибка", "Для создания многоугольника нужно минимум 3 стороны");
            return;
        }
        
        if (nameEdit->text().isEmpty()) {
            QMessageBox::warning(&dialog, "Ошибка", "Введите название фигуры");
            return;
        }
        
        Polygon* polygon = new Polygon(nameEdit->text());
        polygon->setId(getNextId());
        
        QVector<QPoint> relPoints;
        relPoints.append(QPoint(0, 0));
        
        QPoint currentPoint(0, 0);
        double currentAngle = 0;
        
        for (int i = 0; i < tempLengths.size(); ++i) {
            double angleRad = currentAngle * M_PI / 180.0;
            currentPoint = QPoint(
                currentPoint.x() + static_cast<int>(tempLengths[i] * cos(angleRad)),
                currentPoint.y() + static_cast<int>(tempLengths[i] * sin(angleRad))
            );
            relPoints.append(currentPoint);
            
            if (i < tempAngles.size() - 1) {
                currentAngle += (180 - tempAngles[i]);
                while (currentAngle >= 360) currentAngle -= 360;
                while (currentAngle < 0) currentAngle += 360;
            }
        }
        
        for (int i = 0; i < relPoints.size() - 1; ++i) {
            polygon->addVertex(relPoints[i]);
        }
        
        polygon->closePolygon();
        
        for (int i = 0; i < tempAngles.size() && i < polygon->edgeColors.size(); ++i) {
            polygon->setSideColor(i, tempColors[i]);
            polygon->setSideWidth(i, tempWidths[i]);
        }
        
        QRect bounds = polygon->getBoundingRect();
        QPoint shapeCenter = bounds.center();
        QPoint center = getCenterPosition();
        
        int dx = center.x() - shapeCenter.x();
        int dy = center.y() - shapeCenter.y();
        polygon->move(dx, dy);
        
        m_shapes.append(polygon);
        addToShapeList(polygon);
        onShapeSelected(polygon);
        
        dialog.accept();
    });
    
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    dialog.exec();
}
void MainWindow::createEllipseDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Создание эллипса/окружности");
    dialog.setModal(true);
    dialog.resize(450, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* titleLabel = new QLabel("СОЗДАНИЕ ЭЛЛИПСА");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Название:"));
    QLineEdit* nameEdit = new QLineEdit("Эллипс");
    nameLayout->addWidget(nameEdit);
    layout->addLayout(nameLayout);
    
    QHBoxLayout* rxLayout = new QHBoxLayout();
    rxLayout->addWidget(new QLabel("Радиус X:"));
    QSpinBox* rxSpin = new QSpinBox();
    rxSpin->setRange(20, 5000);
    rxSpin->setValue(80);
    rxLayout->addWidget(rxSpin);
    layout->addLayout(rxLayout);
    
    QHBoxLayout* ryLayout = new QHBoxLayout();
    ryLayout->addWidget(new QLabel("Радиус Y:"));
    QSpinBox* rySpin = new QSpinBox();
    rySpin->setRange(20, 5000);
    rySpin->setValue(60);
    ryLayout->addWidget(rySpin);
    layout->addLayout(ryLayout);
    
    QHBoxLayout* angleLayout = new QHBoxLayout();
    angleLayout->addWidget(new QLabel("Угол поворота:"));
    QSpinBox* angleSpin = new QSpinBox();
    angleSpin->setRange(0, 359);
    angleSpin->setValue(0);
    angleSpin->setSuffix("°");
    angleLayout->addWidget(angleSpin);
    layout->addLayout(angleLayout);
    
    QCheckBox* isCircleCheck = new QCheckBox("Окружность (равные радиусы)");
    connect(isCircleCheck, &QCheckBox::toggled, [rxSpin, rySpin](bool checked) {
        if (checked) {
            rySpin->setValue(rxSpin->value());
            rySpin->setEnabled(false);
        } else {
            rySpin->setEnabled(true);
        }
    });
    layout->addWidget(isCircleCheck);
    
    QGroupBox* fillGroup = new QGroupBox("Заливка");
    QVBoxLayout* fillLayout = new QVBoxLayout(fillGroup);
    
    QCheckBox* fillCheck = new QCheckBox("Включить заливку");
    fillCheck->setChecked(true);
    fillLayout->addWidget(fillCheck);
    
    QHBoxLayout* fillColorLayout = new QHBoxLayout();
    fillColorLayout->addWidget(new QLabel("Цвет заливки:"));
    QPushButton* fillColorBtn = new QPushButton();
    fillColorBtn->setFixedSize(50, 25);
    fillColorBtn->setStyleSheet("background-color: #ADD8E6;");
    QColor fillColor = QColor(173, 216, 230);
    
    connect(fillColorBtn, &QPushButton::clicked, [&fillColor, fillColorBtn, this]() {
        QColor color = QColorDialog::getColor(fillColor, this);
        if (color.isValid()) {
            fillColor = color;
            fillColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        }
    });
    
    fillColorLayout->addWidget(fillColorBtn);
    fillColorLayout->addStretch();
    fillLayout->addLayout(fillColorLayout);
    layout->addWidget(fillGroup);
    
    QGroupBox* edgeGroup = new QGroupBox("Контур");
    QVBoxLayout* edgeLayout = new QVBoxLayout(edgeGroup);
    
    QHBoxLayout* edgeColorLayout = new QHBoxLayout();
    edgeColorLayout->addWidget(new QLabel("Цвет линии:"));
    QPushButton* edgeColorBtn = new QPushButton();
    edgeColorBtn->setFixedSize(50, 25);
    edgeColorBtn->setStyleSheet("background-color: black;");
    QColor edgeColor = Qt::black;
    
    connect(edgeColorBtn, &QPushButton::clicked, [&edgeColor, edgeColorBtn, this]() {
        QColor color = QColorDialog::getColor(edgeColor, this);
        if (color.isValid()) {
            edgeColor = color;
            edgeColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        }
    });
    
    edgeColorLayout->addWidget(edgeColorBtn);
    edgeColorLayout->addStretch();
    edgeLayout->addLayout(edgeColorLayout);
    
    QHBoxLayout* edgeWidthLayout = new QHBoxLayout();
    edgeWidthLayout->addWidget(new QLabel("Толщина линии:"));
    QSpinBox* edgeWidthSpin = new QSpinBox();
    edgeWidthSpin->setRange(1, 9999);
    edgeWidthSpin->setValue(3);
    edgeWidthLayout->addWidget(edgeWidthSpin);
    edgeWidthLayout->addStretch();
    edgeLayout->addLayout(edgeWidthLayout);
    
    layout->addWidget(edgeGroup);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        if (nameEdit->text().isEmpty()) {
            QMessageBox::warning(&dialog, "Ошибка", "Введите название");
            return;
        }
        
        Ellipse* ellipse = new Ellipse(nameEdit->text());
        ellipse->setId(getNextId());
        ellipse->setRadiusX(rxSpin->value());
        ellipse->setRadiusY(rySpin->value());
        ellipse->setAngle(angleSpin->value());
        
        QPoint centerPoint = getCenterPosition();
        ellipse->setCenter(centerPoint);
        ellipse->setAnchorPoint(centerPoint);
        
        ellipse->setFill(fillCheck->isChecked());
        ellipse->setFillColor(fillColor);
        ellipse->edgeColors[0] = edgeColor;
        ellipse->edgeWidths[0] = edgeWidthSpin->value();
        
        m_shapes.append(ellipse);
        addToShapeList(ellipse);
        onShapeSelected(ellipse);
        
        dialog.accept();
    });
    
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    dialog.exec();
}

    
    
    
    
    

void MainWindow::createPropertiesPanel()
{
    m_propertiesScrollArea = new QScrollArea(this);
    m_propertiesScrollArea->setGeometry(width() - 380, 20, 360, height() - 100);
    m_propertiesScrollArea->setStyleSheet("QScrollArea { background-color: #f0f0f0; border: 1px solid gray; }");
    m_propertiesScrollArea->setWidgetResizable(true);
    m_propertiesScrollArea->hide();
    
    m_propertiesPanel = new QWidget();
    m_propertiesScrollArea->setWidget(m_propertiesPanel);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_propertiesPanel);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("СВОЙСТВА");
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    
    QPushButton* closeBtn = new QPushButton("X");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setStyleSheet("QPushButton { background-color: red; color: white; }");
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::hidePropertiesPanel);
    
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeBtn);
    mainLayout->addLayout(titleLayout);
    
    QGroupBox* nameGroup = new QGroupBox("Название и ID");
    QVBoxLayout* nameLayout = new QVBoxLayout(nameGroup);
    
    QLabel* idLabel = new QLabel();
    idLabel->setObjectName("idLabel");
    idLabel->setStyleSheet("background-color: white; padding: 4px; font-family: monospace;");
    nameLayout->addWidget(idLabel);
    
    QHBoxLayout* nameEditLayout = new QHBoxLayout();
    QLineEdit* nameEdit = new QLineEdit();
    nameEdit->setObjectName("nameEdit");
    
    QPushButton* renameBtn = new QPushButton("Изменить");
    renameBtn->setFixedWidth(70);
    
    nameEditLayout->addWidget(nameEdit, 1);
    nameEditLayout->addWidget(renameBtn);
    nameLayout->addLayout(nameEditLayout);
    
    connect(renameBtn, &QPushButton::clicked, [this, nameEdit, idLabel]() {
        if (m_currentShape && !nameEdit->text().isEmpty()) {
            m_currentShape->setName(nameEdit->text());
            updateShapesList();
            m_needsRedraw = true;
        }
    });
    
    mainLayout->addWidget(nameGroup);
    
    QGroupBox* anchorGroup = new QGroupBox("Точка привязки");
    QVBoxLayout* anchorLayout = new QVBoxLayout(anchorGroup);
    
    QLabel* coordsLabel = new QLabel("X = 0, Y = 0");
    coordsLabel->setObjectName("coordsLabel");
    coordsLabel->setStyleSheet("background-color: white; padding: 4px; border: 1px solid gray;");
    anchorLayout->addWidget(coordsLabel);
    
    QHBoxLayout* coordLayout = new QHBoxLayout();
    QLabel* xLabel = new QLabel("X:");
    QSpinBox* spinX = new QSpinBox();
    spinX->setObjectName("spinX");
    spinX->setRange(0, 5000);
    
    QLabel* yLabel = new QLabel("Y:");
    QSpinBox* spinY = new QSpinBox();
    spinY->setObjectName("spinY");
    spinY->setRange(0, 5000);
    
    coordLayout->addWidget(xLabel);
    coordLayout->addWidget(spinX);
    coordLayout->addWidget(yLabel);
    coordLayout->addWidget(spinY);
    coordLayout->addStretch();
    anchorLayout->addLayout(coordLayout);
    
    QPushButton* setAnchorBtn = new QPushButton("Установить");
    connect(setAnchorBtn, &QPushButton::clicked, [this, spinX, spinY]() {
        if (m_currentShape) {
            QPoint newPoint(spinX->value(), spinY->value());
            if (m_currentShape->isPointInside(newPoint)) {
                m_currentShape->moveAnchorPoint(newPoint);
                m_needsRedraw = true;
                updatePropertiesPanel();
            }
        }
    });
    anchorLayout->addWidget(setAnchorBtn);
    mainLayout->addWidget(anchorGroup);
    
    QGroupBox* scaleGroup = new QGroupBox("Масштаб");
    QHBoxLayout* scaleLayout = new QHBoxLayout(scaleGroup);
    
    QLabel* scaleLabel = new QLabel("Масштаб:");
    QComboBox* scaleCombo = new QComboBox();
    scaleCombo->addItems({"0.5x", "1x", "1.5x", "2x", "3x"});
    
    connect(scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (!m_currentShape) return;
        double factors[] = {0.5, 1.0, 1.5, 2.0, 3.0};
        m_currentShape->scale(factors[index]);
        m_needsRedraw = true;
        updatePropertiesPanel();
    });
    
    scaleLayout->addWidget(scaleLabel);
    scaleLayout->addWidget(scaleCombo);
    scaleLayout->addStretch();
    mainLayout->addWidget(scaleGroup);
    
    QGroupBox* fillGroup = new QGroupBox("Заливка");
    QVBoxLayout* fillLayout = new QVBoxLayout(fillGroup);
    
    QCheckBox* fillCheck = new QCheckBox("Включить заливку");
    fillCheck->setObjectName("fillCheck");
    fillLayout->addWidget(fillCheck);
    
    QHBoxLayout* fillColorLayout = new QHBoxLayout();
    QLabel* fillColorLabel = new QLabel("Цвет:");
    QPushButton* fillColorBtn = new QPushButton();
    fillColorBtn->setObjectName("fillColorBtn");
    fillColorBtn->setFixedSize(70, 25);
    
    fillColorLayout->addWidget(fillColorLabel);
    fillColorLayout->addWidget(fillColorBtn);
    fillColorLayout->addStretch();
    fillLayout->addLayout(fillColorLayout);
    mainLayout->addWidget(fillGroup);
    
    m_sideTabs = new QTabWidget();
    m_sideTabs->setVisible(false);
    mainLayout->addWidget(m_sideTabs);
    
    mainLayout->addStretch();
}

void MainWindow::clearAll()
{
    int result = QMessageBox::question(this, "Подтверждение", 
        "Очистить весь экран от всех фигур?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result != QMessageBox::Yes) return;
    
    for (Shape* shape : m_shapes) {
        delete shape;
    }
    m_shapes.clear();
    m_currentShape = nullptr;
    
    if (m_shapesContainer) {
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_shapesContainer->layout());
        if (layout) {
            while (layout->count() > 1) {
                QLayoutItem* item = layout->takeAt(0);
                if (item) {
                    if (item->widget()) delete item->widget();
                    delete item;
                }
            }
        }
    }
    
    if (m_isPropertiesVisible) {
        m_propertiesScrollArea->hide();
        m_isPropertiesVisible = false;
    }
    
    m_needsRedraw = true;
    update();
}
void MainWindow::loadExternalPlugin()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Загрузить плагин с фигурой",
        QDir::homePath(),
        "Shared libraries (*.dll *.so *.dylib)");
    
    if (filename.isEmpty()) return;
    
    if (ShapeLoader::instance().loadPlugin(filename)) {
        QMessageBox::information(this, "Успех", "Плагин загружен");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить плагин");
    }
}

void MainWindow::startCompositeCreation()
{
    if (m_shapes.isEmpty()) {
        QMessageBox::information(this, "Внимание", "Нет фигур для создания составной");
        return;
    }
    
    if (m_currentShape) {
        m_currentShape->setSelected(false);
        m_currentShape = nullptr;
    }
    
    m_selectedForComposite.clear();
    m_isCreatingComposite = true;
    
    for (Shape* shape : m_shapes) {
        CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
        if (composite) {
            composite->setSelectableOnly(true);
        }
    }
    
    m_cancelCompositeBtn->show();
    m_finishCompositeBtn->show();
    
    if (m_isPropertiesVisible) {
        m_propertiesScrollArea->hide();
        m_isPropertiesVisible = false;
    }
    
    QMessageBox::information(this, "Режим создания составной фигуры", 
        "Кликайте на фигуры, которые хотите объединить.\n"
        "Выбранные фигуры будут подсвечиваться зеленым.\n"
        "Нажмите 'Готово' для завершения или 'Отмена' для отмены.");
    
    m_needsRedraw = true;
    update();
}

void MainWindow::cancelCompositeCreation()
{
    m_isCreatingComposite = false;
    m_selectedForComposite.clear();
    
    for (Shape* shape : m_shapes) {
        CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
        if (composite) {
            composite->setSelectableOnly(false);
        }
    }
    
    m_cancelCompositeBtn->hide();
    m_finishCompositeBtn->hide();
    
    m_needsRedraw = true;
    update();
}
void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (m_isCreatingComposite) {
        Shape* clickedShape = nullptr;
        
        for (int i = m_shapes.size() - 1; i >= 0; --i) {
            Shape* shape = m_shapes[i];
            if (!shape || !shape->isVisible()) continue;
            
            CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
            
            if (composite && !(event->modifiers() & Qt::ControlModifier)) {
                continue;
            }
            
            if (shape->hitTest(event->pos())) {
                clickedShape = shape;
                break;
            }
        }
        
        if (clickedShape) {
            if (m_selectedForComposite.contains(clickedShape)) {
                m_selectedForComposite.removeOne(clickedShape);
            } else {
                m_selectedForComposite.append(clickedShape);
            }
            m_needsRedraw = true;
            update();
        }
        return;
    }
    
    if (m_isPropertiesVisible && event->pos().x() > m_propertiesScrollArea->x()) {
        return;
    }
    
    if (m_currentShape) {
        Ellipse* ellipse = dynamic_cast<Ellipse*>(m_currentShape);
        if (ellipse && ellipse->isSelected()) {
            m_activeFocusHandle = ellipse->getFocusHandleIndex(event->pos());
            if (m_activeFocusHandle >= 0) {
                m_isDraggingFocus = true;
                m_lastMousePos = event->pos();
                return;
            }
        }
        
        m_activeResizeHandle = m_currentShape->hitTestResizeHandle(event->pos());
        if (m_activeResizeHandle >= 0) {
            m_isResizing = true;
            m_resizeStartPos = event->pos();
            return;
        }
        
        if (m_currentShape->hitTestAnchorHandle(event->pos())) {
            m_isDraggingAnchor = true;
            m_lastMousePos = event->pos();
            return;
        }
        
        if (m_currentShape->hitTest(event->pos())) {
            m_isDragging = true;
            m_lastMousePos = event->pos();
            return;
        }
    }
    
    Shape* clickedShape = nullptr;
    for (int i = m_shapes.size() - 1; i >= 0; --i) {
        if (m_shapes[i] && m_shapes[i]->isVisible() && m_shapes[i]->hitTest(event->pos())) {
            clickedShape = m_shapes[i];
            break;
        }
    }
    
    if (clickedShape) {
        onShapeSelected(clickedShape);
        
        Ellipse* ellipse = dynamic_cast<Ellipse*>(m_currentShape);
        if (ellipse && ellipse->isSelected()) {
            m_activeFocusHandle = ellipse->getFocusHandleIndex(event->pos());
            if (m_activeFocusHandle >= 0) {
                m_isDraggingFocus = true;
                m_lastMousePos = event->pos();
                return;
            }
        }
        
        m_activeResizeHandle = m_currentShape->hitTestResizeHandle(event->pos());
        if (m_activeResizeHandle >= 0) {
            m_isResizing = true;
            m_resizeStartPos = event->pos();
        } else if (m_currentShape->hitTestAnchorHandle(event->pos())) {
            m_isDraggingAnchor = true;
            m_lastMousePos = event->pos();
        } else {
            m_isDragging = true;
            m_lastMousePos = event->pos();
        }
    } else if (m_currentShape) {
        m_currentShape->setSelected(false);
        m_currentShape = nullptr;
        
        if (m_isPropertiesVisible) {
            m_propertiesScrollArea->hide();
            m_isPropertiesVisible = false;
        }
        
        m_needsRedraw = true;
    }
    
    update();
}
void MainWindow::finishCompositeCreation()
{
    if (m_selectedForComposite.size() < 2) {
        QMessageBox::warning(this, "Ошибка", "Выберите хотя бы 2 фигуры");
        return;
    }
    
    showCompositeNameDialog();
    
    for (Shape* shape : m_shapes) {
        CompositeShape* composite = dynamic_cast<CompositeShape*>(shape);
        if (composite) {
            composite->setSelectableOnly(false);
        }
    }
}

void MainWindow::showCompositeNameDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Название составной фигуры",
        "Введите название:", QLineEdit::Normal,
        "Составная фигура", &ok);
    
    if (ok && !name.isEmpty()) {
        CompositeShape* composite = new CompositeShape(name);
        composite->setId(getNextId());
        
        for (Shape* shape : m_selectedForComposite) {
            Shape* copy = copyShapeRecursive(shape);
            if (copy) {
                composite->addShape(copy);
            }
        }
        
        for (Shape* shape : m_selectedForComposite) {
            int idx = m_shapes.indexOf(shape);
            if (idx != -1) {
                m_shapes.removeAt(idx);
                delete shape;
            }
        }
        
        QPoint center = getCenterPosition();
        composite->setAnchorPoint(center);
        
        m_shapes.append(composite);
        addToShapeList(composite);
        onShapeSelected(composite);
    }
    
    m_isCreatingComposite = false;
    m_selectedForComposite.clear();
    
    m_cancelCompositeBtn->hide();
    m_finishCompositeBtn->hide();
    
    m_needsRedraw = true;
    update();
}
Shape* MainWindow::copyShapeRecursive(Shape* shape)
{
    if (!shape) return nullptr;
    
    if (Polygon* polygon = dynamic_cast<Polygon*>(shape)) {
        Polygon* copy = new Polygon(polygon->getName());
        copy->setId(polygon->getId());
        for (const QPoint& p : polygon->points) {
            copy->addVertex(p);
        }
        if (polygon->isClosed()) {
            copy->closePolygon();
        }
        copy->edgeColors = polygon->edgeColors;
        copy->edgeWidths = polygon->edgeWidths;
        copy->setFill(polygon->hasFill());
        copy->setFillColor(polygon->getFillColor());
        copy->setAnchorPoint(polygon->getAnchorPoint());
        return copy;
    }
    else if (Ellipse* ellipse = dynamic_cast<Ellipse*>(shape)) {
        Ellipse* copy = new Ellipse(ellipse->getName());
        copy->setId(ellipse->getId());
        copy->setCenter(ellipse->getCenter());
        copy->setRadiusX(ellipse->getRadiusX());
        copy->setRadiusY(ellipse->getRadiusY());
        copy->setAngle(ellipse->getAngle());  // УЖЕ ДОЛЖНО БЫТЬ
        copy->edgeColors = ellipse->edgeColors;
        copy->edgeWidths = ellipse->edgeWidths;
        copy->setFill(ellipse->hasFill());
        copy->setFillColor(ellipse->getFillColor());
        copy->setAnchorPoint(ellipse->getAnchorPoint());
        return copy;
    }
    else if (CompositeShape* composite = dynamic_cast<CompositeShape*>(shape)) {
        CompositeShape* copy = new CompositeShape(composite->getName());
        copy->setId(composite->getId());
        
        const QList<Shape*>& subShapes = composite->getShapes();
        for (Shape* subShape : subShapes) {
            Shape* subCopy = copyShapeRecursive(subShape);
            if (subCopy) {
                copy->addShape(subCopy);
            }
        }
        
        copy->setFill(composite->hasFill());
        copy->setFillColor(composite->getFillColor());
        copy->setAnchorPoint(composite->getAnchorPoint());
        return copy;
    }
    
    return nullptr;
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete && m_currentShape) {
        int index = m_shapes.indexOf(m_currentShape);
        if (index != -1) {
            Shape* toDelete = m_currentShape;
            m_currentShape = nullptr;
            
            m_shapes.removeAt(index);
            delete toDelete;
            
            updateShapesList();
            
            if (m_isPropertiesVisible) {
                m_propertiesScrollArea->hide();
                m_isPropertiesVisible = false;
            }
            
            m_needsRedraw = true;
            update();
        }
    } 
    else if (event->key() == Qt::Key_Escape) {
        if (m_isCreatingComposite) {
            cancelCompositeCreation();
        } else if (m_isPropertiesVisible) {
            m_propertiesScrollArea->hide();
            m_isPropertiesVisible = false;
            m_needsRedraw = true;
            update();
        }
    } 
    else if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_currentShape) return;
    
    CompositeShape* composite = dynamic_cast<CompositeShape*>(m_currentShape);
    if (!composite) return;
    
    Shape* subShape = composite->hitTestSubShape(event->pos());
    if (subShape) {
        QMenu menu(this);
        QAction* removeAction = menu.addAction("Удалить из составной");
        
        connect(removeAction, &QAction::triggered, [this, composite, subShape]() {
            composite->removeShape(subShape);
            updateShapesList();
            m_needsRedraw = true;
            update();
        });
        
        menu.exec(event->globalPos());
    }
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);
    
    for (Shape* shape : m_shapes) {
        if (shape && shape->isVisible()) {
            shape->draw(painter);
        }
    }
    
    if (m_isCreatingComposite) {
        for (Shape* shape : m_selectedForComposite) {
            QRect rect = shape->getBoundingRect();
            
            QPen highlightPen(Qt::green, 3, Qt::SolidLine);
            painter.setPen(highlightPen);
            painter.setBrush(QColor(0, 255, 0, 80));
            painter.drawRect(rect.adjusted(-2, -2, 2, 2));
            
            painter.setBrush(Qt::green);
            painter.setPen(Qt::white);
            painter.drawEllipse(rect.center(), 6, 6);
        }
        
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);
        
        QString hint = QString("Режим создания составной фигуры. Выбрано: %1. Нажмите 'Готово' для объединения")
            .arg(m_selectedForComposite.size());
        painter.drawText(10, height() - 20, hint);
        
        painter.setPen(QColor(100, 100, 100, 100));
        painter.setBrush(Qt::NoBrush);
        for (Shape* shape : m_shapes) {
            if (!m_selectedForComposite.contains(shape) && shape->isVisible()) {
                QRect rect = shape->getBoundingRect();
                painter.drawRect(rect.adjusted(-1, -1, 1, 1));
            }
        }
    }
    
    if (m_currentShape && m_currentShape->isVisible()) {
        m_currentShape->drawVirtualFigure(painter);
        m_currentShape->drawResizeHandles(painter);
        m_currentShape->drawAnchorHandle(painter);
    }
}



void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDraggingFocus && m_currentShape) {
        Ellipse* ellipse = dynamic_cast<Ellipse*>(m_currentShape);
        if (ellipse && m_activeFocusHandle >= 0) {
            ellipse->resizeByFocus(m_activeFocusHandle, event->pos());
            m_lastMousePos = event->pos();
            m_needsRedraw = true;
            
            if (m_isPropertiesVisible) {
                updatePropertiesPanel();
            }
        }
        update();
        return;
    }
    
    if (m_isResizing && m_currentShape && m_activeResizeHandle >= 0) {
        int deltaX = event->pos().x() - m_resizeStartPos.x();
        int deltaY = event->pos().y() - m_resizeStartPos.y();
        
        m_currentShape->resize(m_activeResizeHandle, deltaX, deltaY);
        m_resizeStartPos = event->pos();
        m_needsRedraw = true;
        
        if (m_isPropertiesVisible) {
            updatePropertiesPanel();
        }
        
        update();
        return;
    }
    
    if (m_isDraggingAnchor && m_currentShape) {
        if (m_currentShape->isPointInside(event->pos())) {
            m_currentShape->moveAnchorPoint(event->pos());
            m_lastMousePos = event->pos();
            m_needsRedraw = true;
            
            if (m_isPropertiesVisible) {
                updatePropertiesPanel();
            }
        }
        update();
        return;
    }
    
    if (m_isDragging && m_currentShape) {
        int dx = event->pos().x() - m_lastMousePos.x();
        int dy = event->pos().y() - m_lastMousePos.y();
        
        m_currentShape->move(dx, dy);
        m_lastMousePos = event->pos();
        m_needsRedraw = true;
        
        if (m_isPropertiesVisible) {
            updatePropertiesPanel();
        }
        update();
        return;
    }
}
void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    
    if (m_isDraggingFocus || m_isResizing || m_isDragging || m_isDraggingAnchor) {
        m_isDragging = false;
        m_isResizing = false;
        m_isDraggingAnchor = false;
        m_isDraggingFocus = false;
        m_activeResizeHandle = -1;
        m_activeFocusHandle = -1;
        
        if (m_currentShape) {
            if (m_isPropertiesVisible) {
                updatePropertiesPanel();
            }
        }
        
        m_needsRedraw = true;
        update();
    }
}
void MainWindow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    
    if (m_propertiesScrollArea) {
        m_propertiesScrollArea->setGeometry(width() - 380, 20, 360, height() - 100);
    }
    if (m_closeButton) m_closeButton->move(width() - 60, 10);
    m_needsRedraw = true;
}

void MainWindow::refreshTimerTick()
{
    if (m_needsRedraw) {
        update();
        m_needsRedraw = false;
    }
}