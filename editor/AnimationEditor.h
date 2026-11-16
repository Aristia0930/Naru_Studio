#pragma once
#include "ProjectDocument.h"
#include "ImageAssets.h"
#include "naru/runtime/GameWorld.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

class AnimationSheetView : public QWidget {
public:
    std::function<void(int,int)> chosen;
    explicit AnimationSheetView(QWidget* parent=nullptr):QWidget(parent){setObjectName("animationSheetView");}
    void setSheet(const QImage& image,int width,int height){image_=image;cellW_=width;cellH_=height;setFixedSize(image.isNull()?QSize(240,180):image.size()*zoom_);update();}
    void setSelection(const QRect& rect){selection_=rect;update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),QColor("#30343c"));if(image_.isNull())return;
        p.setRenderHint(QPainter::SmoothPixmapTransform,false);p.drawImage(QRect(QPoint(),image_.size()*zoom_),image_);
        p.setPen(QPen(QColor(255,255,255,125),1));
        for(int x=0;x<=image_.width();x+=cellW_)p.drawLine(x*zoom_,0,x*zoom_,image_.height()*zoom_);
        for(int y=0;y<=image_.height();y+=cellH_)p.drawLine(0,y*zoom_,image_.width()*zoom_,y*zoom_);
        if(selection_.isValid()){p.setPen(QPen(Qt::yellow,2));p.drawRect(QRect(selection_.topLeft()*zoom_,selection_.size()*zoom_).adjusted(1,1,-2,-2));}
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton||image_.isNull())return;
        const QPoint point=event->position().toPoint()/zoom_;
        if(point.x()>=0&&point.y()>=0&&point.x()<image_.width()&&point.y()<image_.height())chosen(point.x()/cellW_*cellW_,point.y()/cellH_*cellH_);
    }
private:
    QImage image_;QRect selection_;int cellW_=32,cellH_=32,zoom_=2;
};

// Edits a copy; the Inspector records one Undo only after Apply.
class AnimationEditor : public QDialog {
public:
    AnimationEditor(ProjectDocument* doc,const naru::SceneObject& source,QWidget* parent=nullptr):QDialog(parent),doc_(doc),object_(source) {
        setWindowTitle(QStringLiteral("캐릭터 애니메이션 — %1").arg(QString::fromStdString(source.name)));resize(1200,800);
        auto* root=new QVBoxLayout(this);auto* split=new QSplitter(this);root->addWidget(split,1);
        auto* panel=new QWidget(split);auto* left=new QVBoxLayout(panel);clips_=new QListWidget(panel);clips_->setObjectName("animationClips");left->addWidget(clips_);
        auto* clipButtons=new QHBoxLayout;left->addLayout(clipButtons);
        button(clipButtons,QStringLiteral("클립 추가"),[this]{store();bool ok=false;auto name=QInputDialog::getText(this,QStringLiteral("클립 추가"),QStringLiteral("이름 (idle, walk, attack_left 등)"),QLineEdit::Normal,"attack",&ok);if(ok&&!name.isEmpty()){naru::AnimationClip clip;clip.name=name.toStdString();object_.animations.push_back(clip);rebuild(int(object_.animations.size()-1));}});
        button(clipButtons,QStringLiteral("삭제"),[this]{if(current_<0)return;object_.animations.erase(object_.animations.begin()+current_);current_=-1;rebuild(0);});
        automatic_=new QCheckBox(QStringLiteral("idle/walk 자동 전환"));automatic_->setChecked(source.automaticAnimation);left->addWidget(automatic_);
        loop_=new QCheckBox(QStringLiteral("이 클립의 기본 반복"));left->addWidget(loop_);
        preview_=new QLabel;preview_->setMinimumSize(280,260);preview_->setAlignment(Qt::AlignCenter);left->addWidget(preview_);
        status_=new QLabel(QStringLiteral("클립을 고르고 미리보기를 누르세요."));status_->setWordWrap(true);left->addWidget(status_);
        auto* previewButtons=new QHBoxLayout;left->addLayout(previewButtons);
        button(previewButtons,QStringLiteral("미리보기 재생"),[this]{startPreview();});button(previewButtons,QStringLiteral("미리보기 중지"),[this]{timer_.stop();previewWorld_.reset();});
        auto* right=new QWidget(split);auto* frameLayout=new QVBoxLayout(right);
        table_=new QTableWidget(0,9);table_->setObjectName("animationFrames");table_->setHorizontalHeaderLabels({"이미지 ID","X","Y","너비","높이","초","기준 X","기준 Y","신호"});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);frameLayout->addWidget(table_);
        auto* frameButtons=new QHBoxLayout;frameLayout->addLayout(frameButtons);
        button(frameButtons,QStringLiteral("프레임 추가"),[this]{appendFrame(naru::AnimationFrame{});});
        button(frameButtons,QStringLiteral("선택 프레임 삭제"),[this]{if(table_->currentRow()>=0)table_->removeRow(table_->currentRow());});
        button(frameButtons,QStringLiteral("위로"),[this]{moveFrame(-1);});button(frameButtons,QStringLiteral("아래로"),[this]{moveFrame(1);});
        auto* importForm=new QFormLayout;frameLayout->addLayout(importForm);
        asset_=new QComboBox;for(const auto& a:doc_->project().assets)if(a.kind=="image")asset_->addItem(QString::fromStdString(a.name)+" ("+QString::fromStdString(a.id)+")",QString::fromStdString(a.id));importForm->addRow(QStringLiteral("가져올 이미지"),asset_);
        cellW_=new QSpinBox;cellW_->setObjectName("animationCellWidth");cellH_=new QSpinBox;cellH_->setObjectName("animationCellHeight");count_=new QSpinBox;for(auto* s:{cellW_,cellH_}){s->setRange(1,16384);s->setValue(32);}count_->setRange(1,4096);count_->setValue(3);
        importForm->addRow(QStringLiteral("격자 칸 너비"),cellW_);importForm->addRow(QStringLiteral("격자 칸 높이"),cellH_);importForm->addRow(QStringLiteral("가져올 프레임 수 (좌상단부터 행 우선)"),count_);
        auto* sheetScroll=new QScrollArea;sheetScroll->setMinimumHeight(220);sheetScroll->setWidgetResizable(false);sheet_=new AnimationSheetView;sheetScroll->setWidget(sheet_);frameLayout->addWidget(sheetScroll,1);
        appendOnClick_=new QCheckBox(QStringLiteral("클릭할 때마다 프레임 추가 (선택 순서대로)"));frameLayout->addWidget(appendOnClick_);
        sheetHint_=new QLabel;frameLayout->addWidget(sheetHint_);
        sheet_->chosen=[this](int x,int y){const auto asset=asset_->currentData().toString();auto image=loadProjectImage(doc_,asset.toStdString());if(image.isNull()||x+cellW_->value()>image.width()||y+cellH_->value()>image.height())return;
            int row=table_->currentRow();if(row<0||appendOnClick_->isChecked()){if(current_<0||table_->rowCount()>=4096)return;naru::AnimationFrame f;f.asset=asset.toStdString();f.x=x;f.y=y;f.width=cellW_->value();f.height=cellH_->value();appendFrame(f);row=table_->rowCount()-1;}
            else {const QString values[]={asset,QString::number(x),QString::number(y),QString::number(cellW_->value()),QString::number(cellH_->value())};for(int col=0;col<5;++col)table_->item(row,col)->setText(values[col]);}
            table_->setCurrentCell(row,0);updateSheet();};
        connect(asset_,&QComboBox::currentIndexChanged,this,[this]{updateSheet();});
        connect(cellW_,&QSpinBox::valueChanged,this,[this]{updateSheet();});connect(cellH_,&QSpinBox::valueChanged,this,[this]{updateSheet();});
        connect(table_,&QTableWidget::currentCellChanged,this,[this]{updateSheet();});connect(table_,&QTableWidget::itemChanged,this,[this]{updateSheet();});
        updateSheet();
        auto* imports=new QHBoxLayout;frameLayout->addLayout(imports);
        button(imports,QStringLiteral("격자 시트 추가"),[this]{importSheet(false);});button(imports,QStringLiteral("4방향 idle/walk 틀 생성"),[this]{importSheet(true);});
        button(imports,QStringLiteral("이미지 한 장 추가"),[this]{auto image=loadProjectImage(doc_,asset_->currentData().toString().toStdString());if(image.isNull())return;naru::AnimationFrame f;f.asset=asset_->currentData().toString().toStdString();f.width=image.width();f.height=image.height();appendFrame(f);});
        auto* hint=new QLabel(QStringLiteral("격자 클릭은 선택 프레임을 바꿉니다. 여러 칸을 순서대로 추가하려면 '클릭할 때마다 프레임 추가'를 켜세요. 좌표는 표에서 세부 조정할 수 있습니다. 기준점은 객체 발밑 기준 0~1, 신호는 프레임 진입 때 발생합니다."));hint->setWordWrap(true);frameLayout->addWidget(hint);
        split->addWidget(panel);split->addWidget(right);split->setSizes({320,880});
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);root->addWidget(buttons);buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("객체에 적용"));buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,[this]{if(validate()){object_.automaticAnimation=automatic_->isChecked();accept();}});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(clips_,&QListWidget::currentRowChanged,this,[this](int row){store();current_=row;load();timer_.stop();previewWorld_.reset();});
        timer_.setInterval(16);connect(&timer_,&QTimer::timeout,this,[this]{if(!previewWorld_)return;previewWorld_->update(.016,naru::InputState{});drawPreview();});
        rebuild(0);
    }
    const naru::SceneObject& object() const{return object_;}
private:
    void updateSheet(){if(!sheet_)return;const auto id=asset_->currentData().toString();auto image=loadProjectImage(doc_,id.toStdString());sheet_->setSheet(image,cellW_->value(),cellH_->value());int row=table_->currentRow();QRect selected;
        if(row>=0&&table_->item(row,0)&&table_->item(row,1)&&table_->item(row,2)&&table_->item(row,3)&&table_->item(row,4)&&table_->item(row,0)->text()==id){bool ok[4];int x=table_->item(row,1)->text().toInt(&ok[0]),y=table_->item(row,2)->text().toInt(&ok[1]),w=table_->item(row,3)->text().toInt(&ok[2]),h=table_->item(row,4)->text().toInt(&ok[3]);if(ok[0]&&ok[1]&&ok[2]&&ok[3]&&w>0&&h>0)selected=QRect(x,y,w,h);}
        sheet_->setSelection(selected);sheetHint_->setText(image.isNull()?QStringLiteral("이미지를 선택하세요."):QStringLiteral("이미지 %1×%2 · 격자 %3×%4 · 선택 프레임 %5").arg(image.width()).arg(image.height()).arg(cellW_->value()).arg(cellH_->value()).arg(row<0?QStringLiteral("없음 (클릭하면 추가)"):QString::number(row+1)));}
    void button(QHBoxLayout* row,const QString& text,const std::function<void()>& action){auto* b=new QPushButton(text);b->setAutoDefault(false);row->addWidget(b);connect(b,&QPushButton::clicked,this,action);}
    void appendFrame(const naru::AnimationFrame& f){int row=table_->rowCount();table_->insertRow(row);QStringList values{QString::fromStdString(f.asset),QString::number(f.x),QString::number(f.y),QString::number(f.width),QString::number(f.height),QString::number(f.seconds),QString::number(f.anchorX),QString::number(f.anchorY),QString::fromStdString(f.signal)};for(int i=0;i<9;++i)table_->setItem(row,i,new QTableWidgetItem(values[i]));}
    void store(){if(current_<0||current_>=int(object_.animations.size()))return;auto& c=object_.animations[current_];c.loop=loop_->isChecked();c.frames.clear();
        for(int row=0;row<table_->rowCount();++row){auto text=[&](int col){return table_->item(row,col)?table_->item(row,col)->text():QString();};bool valid=true;auto number=[&](int col){bool ok=false;double value=text(col).toDouble(&ok);valid&=ok;return value;};
            naru::AnimationFrame f;f.asset=text(0).toStdString();double x=number(1),y=number(2),w=number(3),h=number(4);valid&=std::isfinite(x)&&std::isfinite(y)&&std::isfinite(w)&&std::isfinite(h)&&x>=0&&x<=100000&&y>=0&&y<=100000&&w>=1&&w<=16384&&h>=1&&h<=16384&&std::floor(x)==x&&std::floor(y)==y&&std::floor(w)==w&&std::floor(h)==h;
            f.x=valid?int(x):0;f.y=valid?int(y):0;f.width=valid?int(w):0;f.height=valid?int(h):0;f.seconds=number(5);f.anchorX=number(6);f.anchorY=number(7);f.signal=text(8).toStdString();if(!valid)f.width=0;c.frames.push_back(f);}
    }
    void load(){table_->setRowCount(0);if(current_<0||current_>=int(object_.animations.size()))return;const auto& c=object_.animations[current_];loop_->setChecked(c.loop);for(const auto& f:c.frames)appendFrame(f);}
    void rebuild(int selected){clips_->blockSignals(true);clips_->clear();for(const auto& c:object_.animations)clips_->addItem(QString::fromStdString(c.name));clips_->blockSignals(false);current_=-1;clips_->setCurrentRow(std::clamp(selected,0,std::max(0,clips_->count()-1)));}
    void moveFrame(int delta){int row=table_->currentRow(),next=row+delta;if(row<0||next<0||next>=table_->rowCount())return;for(int col=0;col<9;++col){auto* a=table_->takeItem(row,col);auto* b=table_->takeItem(next,col);table_->setItem(row,col,b);table_->setItem(next,col,a);}table_->setCurrentCell(next,0);}
    bool validate(){store();try{naru::validateAnimations(object_.animations);for(const auto& c:object_.animations)for(const auto& f:c.frames){const auto image=loadProjectImage(doc_,f.asset);if(image.isNull()||f.x+f.width>image.width()||f.y+f.height>image.height())throw std::invalid_argument("프레임 이미지가 없거나 자르기 영역이 이미지 밖입니다: "+c.name+" / "+f.asset);}return true;}catch(const std::exception& e){QMessageBox::warning(this,QStringLiteral("애니메이션 설정 오류"),QString::fromUtf8(e.what()));return false;}}
    void importSheet(bool fourDirections){store();auto asset=asset_->currentData().toString().toStdString();auto image=loadProjectImage(doc_,asset);int w=cellW_->value(),h=cellH_->value(),columns=image.width()/w,rows=image.height()/h;if(columns<=0||rows<=0||count_->value()>columns*rows){QMessageBox::warning(this,QStringLiteral("격자"),QStringLiteral("이미지/칸 크기/프레임 수를 확인하세요."));return;}
        if(fourDirections){if(rows<4||object_.animations.size()+8>256)return;for(const char* direction:{"down","left","right","up"})for(const char* base:{"idle","walk"})if(naru::findAnimation(object_.animations,std::string(base)+"_"+direction)){QMessageBox::warning(this,QStringLiteral("클립 중복"),QStringLiteral("같은 이름의 방향 클립이 이미 있습니다."));return;}
            int y=0;for(const char* direction:{"down","left","right","up"}){for(const char* base:{"idle","walk"}){naru::AnimationClip c;c.name=std::string(base)+"_"+direction;for(int x=0;x<(std::string(base)=="idle"?1:std::min(columns,count_->value()));++x){naru::AnimationFrame f;f.asset=asset;f.x=x*w;f.y=y*h;f.width=w;f.height=h;c.frames.push_back(f);}object_.animations.push_back(c);}++y;}rebuild(0);
        }else{if(current_<0)return;for(int index=0;index<count_->value()&&table_->rowCount()<4096;++index){naru::AnimationFrame f;f.asset=asset;f.x=index%columns*w;f.y=index/columns*h;f.width=w;f.height=h;appendFrame(f);}}
    }
    void startPreview(){if(!validate()||current_<0)return;naru::Project p;naru::Scene scene;scene.id="preview";auto o=object_;o.automaticAnimation=automatic_->isChecked();o.id="actor";o.x=0;o.y=0;scene.objects={o};p.scenes={scene};previewWorld_=std::make_unique<naru::GameWorld>(p,naru::ScriptRunner{});previewWorld_->startGame("preview");previewWorld_->playObjectAnimation("actor",object_.animations[current_].name,loop_->isChecked(),false);timer_.start();drawPreview();}
    void drawPreview(){const auto* o=previewWorld_->object("actor");const auto* c=naru::findAnimation(o->animations,o->animationState.clip);if(!c||c->frames.empty())return;const auto& f=c->frames[o->animationState.frame];auto image=loadProjectImage(doc_,f.asset).copy(f.x,f.y,f.width,f.height);
        QPixmap pixmap(preview_->size());pixmap.fill(QColor("#30343c"));QPainter painter(&pixmap);double scale=std::min({1.,(pixmap.width()-30.)/f.width,(pixmap.height()-30.)/f.height});QPointF anchor(pixmap.width()/2.,pixmap.height()-25.);painter.setRenderHint(QPainter::SmoothPixmapTransform,object_.image.smooth);painter.drawImage(QRectF(anchor.x()-f.anchorX*f.width*scale,anchor.y()-f.anchorY*f.height*scale,f.width*scale,f.height*scale),image);painter.setPen(Qt::yellow);painter.drawEllipse(anchor,3,3);painter.end();preview_->setPixmap(pixmap);
        QString signal;for(const auto& s:previewWorld_->takeAnimationSignals())signal=QString::fromStdString(s.signal);status_->setText(QStringLiteral("%1 · 프레임 %2/%3%4").arg(QString::fromStdString(c->name)).arg(o->animationState.frame+1).arg(c->frames.size()).arg(signal.isEmpty()?QString():QStringLiteral(" · 신호: ")+signal));}
    ProjectDocument* doc_;naru::SceneObject object_;int current_=-1;QListWidget* clips_;QTableWidget* table_;QComboBox* asset_;QCheckBox *automatic_,*loop_,*appendOnClick_;QLabel *preview_,*status_,*sheetHint_;AnimationSheetView* sheet_=nullptr;QSpinBox *cellW_,*cellH_,*count_;QTimer timer_;std::unique_ptr<naru::GameWorld> previewWorld_;
};
