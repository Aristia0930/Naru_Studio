#pragma once
#include "ProjectDocument.h"
#include "UndoCommands.h"
#include "ImageAssets.h"
#include "naru/project/TileMap.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>


inline void paintMapTiles(QPainter* painter, const naru::Scene& scene, const std::vector<QImage>& images, bool above) {
    int size = scene.tileSize, columns = (scene.width + size - 1) / size;
    for (std::size_t i = 0; i < scene.tileLayers.size(); ++i) {
        const auto& layer = scene.tileLayers[i]; const auto& image = images[i];
        if (!layer.visible || layer.above != above || image.width() < size || image.height() < size) continue;
        int atlasColumns = image.width() / size;
        for (const auto& [cell, tile] : layer.cells) {
            QRect dest((cell % columns) * size, (cell / columns) * size, size, size);
            QRect source((tile % atlasColumns) * size, (tile / atlasColumns) * size, size, size);
            if (source.bottom() >= image.height()) continue;
            if (painter->hasClipping() && !painter->clipBoundingRect().intersects(dest)) continue;
            painter->drawImage(dest, image, source);
        }
    }
}

class TileMapCanvas : public QGraphicsView {
public:
    naru::Scene map;
    std::vector<QImage> images;
    int layer = -1, tile = 0;
    bool erase = false;
    std::function<void(const naru::Scene&)> commit;
    explicit TileMapCanvas(QWidget* parent = nullptr) : QGraphicsView(parent) {
        setScene(new QGraphicsScene(this)); setBackgroundBrush(QColor("#30343c"));
        setDragMode(QGraphicsView::NoDrag);
    }
    void refresh() { setSceneRect(0, 0, map.width, map.height); viewport()->update(); }
protected:
    void drawBackground(QPainter* painter, const QRectF&) override {
        painter->fillRect(QRectF(0, 0, map.width, map.height), QColor(QString::fromStdString(map.background)));
        painter->save(); painter->setClipRect(QRect(0, 0, map.width, map.height));
        paintMapTiles(painter, map, images, false);
        for (const auto& o : map.objects) if (o.visible) painter->fillRect(QRectF(o.x, o.y, o.width, o.height), QColor(QString::fromStdString(o.color)));
        paintMapTiles(painter, map, images, true);
        auto bounds = mapToScene(viewport()->rect()).boundingRect().intersected(QRectF(0,0,map.width,map.height));
        painter->setPen(QColor(255,255,255,45));
        for (int x = int(bounds.left()) / map.tileSize * map.tileSize; x <= bounds.right(); x += map.tileSize) painter->drawLine(QPointF(x,bounds.top()),QPointF(x,bounds.bottom()));
        for (int y = int(bounds.top()) / map.tileSize * map.tileSize; y <= bounds.bottom(); y += map.tileSize) painter->drawLine(QPointF(bounds.left(),y),QPointF(bounds.right(),y));
        painter->restore();
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
            dragging_ = true; right_ = event->button() == Qt::RightButton; changed_ = false;
            last_ = mapToScene(event->pos()); paintAt(last_); return;
        }
        QGraphicsView::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!dragging_) { QGraphicsView::mouseMoveEvent(event); return; }
        auto point = mapToScene(event->pos());
        double distance = std::max(std::abs(point.x()-last_.x()), std::abs(point.y()-last_.y()));
        int steps = std::min(10000, std::max(1, int(distance / (map.tileSize / 3.))));
        for (int i=1;i<=steps;++i) paintAt(last_ + (point-last_) * (double(i)/steps));
        last_ = point;
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (dragging_) { dragging_ = false; if (changed_ && commit) commit(map); return; }
        QGraphicsView::mouseReleaseEvent(event);
    }
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            double factor=event->angleDelta().y()>0?1.2:1/1.2;
            if (transform().m11()*factor >= .1 && transform().m11()*factor <= 4) scale(factor,factor);
        } else QGraphicsView::wheelEvent(event);
    }
private:
    void paintAt(QPointF point) {
        if (layer < 0 || layer >= int(map.tileLayers.size()) || point.x()<0 || point.y()<0 || point.x()>=map.width || point.y()>=map.height) return;
        auto& current=map.tileLayers[layer]; if (current.locked || !current.visible) return;
        int cell=int(point.y())/map.tileSize*((map.width+map.tileSize-1)/map.tileSize)+int(point.x())/map.tileSize;
        if (erase || right_) changed_ |= current.cells.erase(cell)>0;
        else {
            const auto& image=images[layer];
            if (image.isNull() || tile<0 || tile >= (image.width()/map.tileSize)*(image.height()/map.tileSize) || current.cells.size()>=262144) return;
            auto it=current.cells.find(cell);
            if (it==current.cells.end() || it->second!=tile) {current.cells[cell]=tile;changed_=true;}
        }
        viewport()->update();
    }
    bool dragging_=false, right_=false, changed_=false;
    QPointF last_;
};

class TileMapEditor : public QDialog {
public:
    TileMapEditor(ProjectDocument* doc, const QString& sceneId, QWidget* parent=nullptr) : QDialog(parent), doc_(doc), id_(sceneId) {
        setWindowTitle(QStringLiteral("레이어 타일맵 편집")); resize(1200,820);
        auto* root=new QVBoxLayout(this); auto* split=new QSplitter(this); root->addWidget(split);
        auto* panel=new QWidget(split); auto* left=new QVBoxLayout(panel);
        layers_=new QListWidget(panel); layers_->setObjectName("tileLayers"); left->addWidget(new QLabel(QStringLiteral("레이어 (목록 아래가 나중에 그려짐)")));left->addWidget(layers_);
        auto* row=new QHBoxLayout;left->addLayout(row);
        button(row, "추가", [this]{change([](naru::Scene& s){if(s.tileLayers.size()<32) {naru::TileLayer l;l.name="레이어 "+std::to_string(s.tileLayers.size()+1);s.tileLayers.push_back(l);}});layers_->setCurrentRow(layers_->count()-1);});
        button(row, "삭제", [this]{int i=layers_->currentRow();if(i<0)return;if(QMessageBox::question(this,QStringLiteral("레이어 삭제"),QStringLiteral("레이어와 칠한 타일을 삭제할까요? 실행 취소할 수 있습니다."))==QMessageBox::Yes)change([i](naru::Scene& s){s.tileLayers.erase(s.tileLayers.begin()+i);});});
        button(row, "앞으로", [this]{move(-1);}); button(row, "뒤로", [this]{move(1);});
        auto* form=new QFormLayout;left->addLayout(form);
        name_=new QLineEdit;form->addRow(QStringLiteral("레이어 이름"),name_);
        asset_=new QComboBox;asset_->addItem(QStringLiteral("(없음)"),QString());
        for(const auto& a:doc->project().assets)if(a.kind=="image")asset_->addItem(QString::fromStdString(a.name)+" ("+QString::fromStdString(a.id)+")",QString::fromStdString(a.id));
        form->addRow(QStringLiteral("타일셋 이미지"),asset_);
        visible_=new QCheckBox(QStringLiteral("표시 (끄면 게임 충돌도 해제)"));locked_=new QCheckBox(QStringLiteral("편집 잠금"));above_=new QCheckBox(QStringLiteral("캐릭터 위에 표시"));
        for(auto* check:{visible_,locked_,above_})form->addRow(check);
        auto* settings=new QPushButton(QStringLiteral("레이어 설정 적용"));left->addWidget(settings);
        connect(settings,&QPushButton::clicked,this,[this]{int i=layers_->currentRow();if(i<0||name_->text().trimmed().isEmpty())return;
            change([&](naru::Scene& s){auto& l=s.tileLayers[i];l.name=name_->text().trimmed().toStdString();l.asset=asset_->currentData().toString().toStdString();l.visible=visible_->isChecked();l.locked=locked_->isChecked();l.above=above_->isChecked();});});
        palette_=new QListWidget;palette_->setObjectName("tilePalette");palette_->setViewMode(QListView::IconMode);palette_->setIconSize(QSize(40,40));palette_->setMaximumHeight(180);left->addWidget(palette_);
        solid_=new QCheckBox(QStringLiteral("선택 타일은 통과 불가"));left->addWidget(solid_);
        connect(solid_,&QCheckBox::clicked,this,[this](bool checked){int i=layers_->currentRow(),tile=palette_->currentRow();if(i<0||tile<0)return;change([&](naru::Scene&s){auto& v=s.tileLayers[i].solidTiles;std::erase(v,tile);if(checked)v.push_back(tile);});});
        auto* erase=new QCheckBox(QStringLiteral("지우개 (오른쪽 드래그로도 지움)"));left->addWidget(erase);
        canvas_=new TileMapCanvas(split);canvas_->setObjectName("tileCanvas");split->addWidget(panel);split->addWidget(canvas_);split->setSizes({330,870});
        connect(erase,&QCheckBox::toggled,this,[this](bool value){canvas_->erase=value;});
        connect(layers_,&QListWidget::currentRowChanged,this,[this]{refreshLayer();});
        connect(palette_,&QListWidget::currentRowChanged,this,[this](int i){canvas_->tile=i;refreshSolid();});
        auto* dimensions=new QHBoxLayout;root->addLayout(dimensions);
        width_=spin(dimensions,"맵 너비",32,32768);height_=spin(dimensions,"높이",32,32768);size_=spin(dimensions,"타일 크기",8,256);
        width_->setObjectName("tileMapWidth");height_->setObjectName("tileMapHeight");size_->setObjectName("tileMapSize");
        cameraW_=spin(dimensions,"카메라 너비",64,4096);cameraH_=spin(dimensions,"높이",64,4096);
        button(dimensions,"맵/카메라 설정 적용",[this]{
            bool hasTiles=false;for(const auto& l:canvas_->map.tileLayers)hasTiles|=!l.cells.empty();
            if(hasTiles && size_->value()!=canvas_->map.tileSize) {QMessageBox::warning(this,QStringLiteral("타일 크기 변경"),QStringLiteral("칠한 타일이 있으면 타일 크기는 변경할 수 없습니다. 맵 너비·높이는 변경할 수 있습니다."));return;}
            auto* before=doc_->project().findScene(id_.toStdString());if(!before)return;
            auto after=*before;after.tileSize=size_->value();
            const auto summary=naru::resizeTileMap(after,width_->value(),height_->value());
            after.cameraWidth=cameraW_->value();after.cameraHeight=cameraH_->value();
            if(after.width!=before->width || after.height!=before->height) {
                if(QMessageBox::question(this,QStringLiteral("맵 크기 변경 확인"),QStringLiteral("%1 × %2 → %3 × %4 픽셀\n잘릴 타일: %5개 (전체 레이어 합계)\n새 영역 밖 객체: %6개 (위치는 유지)\n좌상단 기준으로 칸 좌표를 유지합니다. 부분적으로 남는 칸은 유지합니다.\n적용 후 실행 취소로 복원할 수 있습니다. 적용할까요?").arg(before->width).arg(before->height).arg(after.width).arg(after.height).arg(qulonglong(summary.croppedTiles)).arg(qulonglong(summary.outsideObjects)),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            }
            if(after!=*before)doc_->undoStack()->push(new SetSceneInfoCommand(doc_,*before,after,-1));});
        auto* bottom=new QHBoxLayout;root->addLayout(bottom);
        button(bottom,"실행 취소",[this]{doc_->undoStack()->undo();});button(bottom,"다시 실행",[this]{doc_->undoStack()->redo();});
        bottom->addWidget(new QLabel(QStringLiteral("왼쪽 드래그: 칠하기 · Ctrl+휠: 확대/축소 · 스크롤바: 이동 · 변경은 즉시 반영, 프로젝트 저장은 별도")));
        button(bottom,"닫기",[this]{accept();});
        canvas_->commit=[this](const naru::Scene& after){auto* before=doc_->project().findScene(id_.toStdString());if(before && *before!=after)doc_->undoStack()->push(new SetSceneInfoCommand(doc_,*before,after,-1));};
        connect(doc_,&ProjectDocument::sceneChanged,this,[this](const QString& id){if(id==id_)refresh();});
        refresh();
    }
private:
    void button(QHBoxLayout* row,const char* text,std::function<void()> action){auto* b=new QPushButton(QString::fromUtf8(text));row->addWidget(b);connect(b,&QPushButton::clicked,this,action);}
    QSpinBox* spin(QHBoxLayout* row,const char* label,int min,int max){row->addWidget(new QLabel(QString::fromUtf8(label)));auto* s=new QSpinBox;s->setRange(min,max);row->addWidget(s);return s;}
    void change(const std::function<void(naru::Scene&)>& action){auto* before=doc_->project().findScene(id_.toStdString());if(!before)return;auto after=*before;action(after);if(after!=*before)doc_->undoStack()->push(new SetSceneInfoCommand(doc_,*before,after,-1));}
    void move(int delta){int i=layers_->currentRow(),next=i+delta;if(i<0||next<0||next>=layers_->count())return;change([&](naru::Scene&s){std::swap(s.tileLayers[i],s.tileLayers[next]);});layers_->setCurrentRow(next);}
    void refresh(){const auto* scene=doc_->project().findScene(id_.toStdString());if(!scene){reject();return;}int selected=layers_->currentRow();
        canvas_->map=*scene;canvas_->images.clear();for(const auto& l:scene->tileLayers)canvas_->images.push_back(loadProjectImage(doc_,l.asset));
        layers_->blockSignals(true);layers_->clear();for(const auto& l:scene->tileLayers)layers_->addItem(QString::fromStdString(l.name)+(l.above?QStringLiteral(" [위]"):QStringLiteral(" [아래]")));layers_->blockSignals(false);
        width_->setValue(scene->width);height_->setValue(scene->height);size_->setValue(scene->tileSize);cameraW_->setValue(scene->cameraWidth?scene->cameraWidth:960);cameraH_->setValue(scene->cameraHeight?scene->cameraHeight:540);
        layers_->setCurrentRow(std::clamp(selected,0,std::max(0,layers_->count()-1)));refreshLayer();canvas_->refresh();}
    void refreshLayer(){int i=layers_->currentRow();canvas_->layer=i;int selected=palette_->currentRow();palette_->clear();if(i<0||i>=int(canvas_->map.tileLayers.size()))return;const auto& l=canvas_->map.tileLayers[i];
        name_->setText(QString::fromStdString(l.name));asset_->setCurrentIndex(std::max(0,asset_->findData(QString::fromStdString(l.asset))));visible_->setChecked(l.visible);locked_->setChecked(l.locked);above_->setChecked(l.above);
        const auto& image=canvas_->images[i];int size=canvas_->map.tileSize,cols=image.width()/size,rows=image.height()/size;
        if(cols*rows>4096){QMessageBox::warning(this,QStringLiteral("타일셋"),QStringLiteral("타일셋은 최대 4096칸까지 편집할 수 있습니다. 타일 크기나 이미지를 조정하세요."));return;}
        for(int tile=0;tile<cols*rows;++tile)palette_->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(image.copy(tile%cols*size,tile/cols*size,size,size))),QString::number(tile)));
        palette_->setCurrentRow(std::clamp(selected,0,std::max(0,palette_->count()-1)));refreshSolid();}
    void refreshSolid(){int i=layers_->currentRow(),tile=palette_->currentRow();if(i<0||i>=int(canvas_->map.tileLayers.size()))return;const auto& v=canvas_->map.tileLayers[i].solidTiles;solid_->setChecked(std::find(v.begin(),v.end(),tile)!=v.end());}
    ProjectDocument* doc_;QString id_;TileMapCanvas* canvas_;QListWidget *layers_,*palette_;QLineEdit* name_;QComboBox* asset_;QCheckBox *visible_,*locked_,*above_,*solid_;QSpinBox *width_,*height_,*size_,*cameraW_,*cameraH_;
};
