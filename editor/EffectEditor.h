#pragma once
#include "ProjectDocument.h"
#include "UndoCommands.h"
#include "ImageAssets.h"
#include "naru/project/ImageLayout.h"
#include "AnimationEditor.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QScrollArea>
#include <QUndoCommand>
#include <QTimer>

class SetEffectsCommand : public QUndoCommand {
public:
    SetEffectsCommand(ProjectDocument* doc,std::vector<naru::Effect> after):doc_(doc),before_(doc->project().effects),after_(std::move(after)){setText(QStringLiteral("이펙트 편집"));}
    void redo() override{doc_->setEffects(after_);}
    void undo() override{doc_->setEffects(before_);}
private:ProjectDocument* doc_;std::vector<naru::Effect> before_,after_;
};

class EffectEditor : public QDialog {
public:
    explicit EffectEditor(ProjectDocument* doc,QWidget* parent=nullptr):QDialog(parent),doc_(doc),effects_(doc->project().effects){
        setWindowTitle(QStringLiteral("이펙트 라이브러리"));resize(1150,750);auto* root=new QVBoxLayout(this);auto* split=new QSplitter;root->addWidget(split,1);
        auto* left=new QWidget;auto* lv=new QVBoxLayout(left);list_=new QListWidget;lv->addWidget(list_);
        auto* add=new QPushButton(QStringLiteral("이펙트 추가"));auto* remove=new QPushButton(QStringLiteral("선택 이펙트 삭제"));lv->addWidget(add);lv->addWidget(remove);split->addWidget(left);
        auto* right=new QWidget;auto* rv=new QVBoxLayout(right);auto* form=new QFormLayout;rv->addLayout(form);
        id_=new QLineEdit;id_->setReadOnly(true);name_=new QLineEdit;form->addRow(QStringLiteral("이펙트 ID"),id_);form->addRow(QStringLiteral("표시 이름"),name_);
        table_=new QTableWidget(0,16);table_->setObjectName("effectFrames");table_->setHorizontalHeaderLabels({"이미지 ID","X","Y","너비","높이","초","이동 X","이동 Y","배율","회전","불투명도","합성","소리 ID","대상 번쩍임","화면 번쩍임","화면 흔들림"});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);rv->addWidget(table_,1);
        auto* row=new QHBoxLayout;rv->addLayout(row);auto* frameAdd=new QPushButton(QStringLiteral("프레임 추가"));auto* frameRemove=new QPushButton(QStringLiteral("선택 프레임 삭제"));row->addWidget(frameAdd);row->addWidget(frameRemove);
        auto* controls=new QFormLayout;rv->addLayout(controls);image_=new QComboBox;for(const auto& a:doc_->project().assets)if(a.kind=="image")image_->addItem(QString::fromStdString(a.name+" ("+a.id+")"),QString::fromStdString(a.id));controls->addRow(QStringLiteral("이미지"),image_);
        width_=new QSpinBox;height_=new QSpinBox;for(auto* s:{width_,height_}){s->setRange(1,16384);s->setValue(32);}controls->addRow(QStringLiteral("격자 칸 너비"),width_);controls->addRow(QStringLiteral("격자 칸 높이"),height_);
        auto* scroll=new QScrollArea;scroll->setMinimumHeight(210);sheet_=new AnimationSheetView;scroll->setWidget(sheet_);rv->addWidget(scroll,1);
        preview_=new QLabel;preview_->setMinimumHeight(160);preview_->setAlignment(Qt::AlignCenter);rv->addWidget(preview_);
        auto* play=new QPushButton(QStringLiteral("대상 위 미리보기 재생"));rv->addWidget(play);
        auto* hint=new QLabel(QStringLiteral("행을 고르고 칸을 클릭하면 이미지·영역을 바꿉니다. 빈 표에서 클릭하면 프레임을 추가합니다. 소리·합성·위치·회전·투명도는 표에서 편집합니다."));hint->setWordWrap(true);rv->addWidget(hint);split->addWidget(right);split->setSizes({230,900});
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("프로젝트에 적용"));buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));root->addWidget(buttons);
        connect(list_,&QListWidget::currentRowChanged,this,[this](int row){previewTimer_.stop();store();current_=row;load();});
        connect(add,&QPushButton::clicked,this,[this]{store();if(effects_.size()>=256)return;bool ok=false;QString id=QInputDialog::getText(this,QStringLiteral("이펙트 추가"),QStringLiteral("이펙트 ID"),QLineEdit::Normal,"fire",&ok);if(!ok||id.isEmpty())return;naru::Effect e;e.id=id.toStdString();e.name=e.id;effects_.push_back(e);rebuild(int(effects_.size()-1));});
        connect(remove,&QPushButton::clicked,this,[this]{if(current_<0)return;effects_.erase(effects_.begin()+current_);current_=-1;rebuild(0);});
        connect(frameAdd,&QPushButton::clicked,this,[this]{if(current_<0)return;naru::EffectFrame f;f.image=image_->currentData().toString().toStdString();append(f);});
        connect(frameRemove,&QPushButton::clicked,this,[this]{if(table_->currentRow()>=0)table_->removeRow(table_->currentRow());});
        connect(play,&QPushButton::clicked,this,[this]{store();previewFrame_=0;drawPreview();previewTimer_.start();});
        connect(&previewTimer_,&QTimer::timeout,this,[this]{if(current_<0||current_>=int(effects_.size())||effects_[current_].frames.empty()){previewTimer_.stop();return;}if(++previewFrame_>=effects_[current_].frames.size()){previewTimer_.stop();return;}drawPreview();});
        auto refresh=[this]{sheet_->setSheet(loadProjectImage(doc_,image_->currentData().toString().toStdString()),width_->value(),height_->value());};
        connect(image_,&QComboBox::currentIndexChanged,this,refresh);connect(width_,&QSpinBox::valueChanged,this,refresh);connect(height_,&QSpinBox::valueChanged,this,refresh);refresh();
        sheet_->chosen=[this](int x,int y){auto image=loadProjectImage(doc_,image_->currentData().toString().toStdString());if(current_<0||x+width_->value()>image.width()||y+height_->value()>image.height())return;int row=table_->currentRow();if(row<0){naru::EffectFrame f;f.image=image_->currentData().toString().toStdString();append(f);row=table_->rowCount()-1;}QString values[]={image_->currentData().toString(),QString::number(x),QString::number(y),QString::number(width_->value()),QString::number(height_->value())};for(int i=0;i<5;++i)table_->item(row,i)->setText(values[i]);table_->setCurrentCell(row,0);};
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,[this]{store();try{auto candidate=doc_->project();candidate.effects=effects_;auto error=naru::validateImages(candidate);if(!error.empty())throw std::invalid_argument(error);for(const auto& old:doc_->project().effects)if(std::none_of(effects_.begin(),effects_.end(),[&](const naru::Effect& e){return e.id==old.id;})){for(const auto& scene:candidate.scenes)for(const auto& object:scene.objects)if(uses(object.event.commands,old.id))throw std::invalid_argument("이펙트를 이벤트에서 사용 중입니다: "+old.id);for(const auto& common:candidate.commonEvents)if(uses(common.event.commands,old.id))throw std::invalid_argument("이펙트를 공통 이벤트에서 사용 중입니다: "+old.id);}for(const auto& e:effects_)for(const auto& f:e.frames){auto image=loadProjectImage(doc_,f.image);if(image.isNull()||f.x+f.width>image.width()||f.y+f.height>image.height())throw std::invalid_argument("이펙트 그림 범위 오류: "+e.id);}}catch(const std::exception& e){QMessageBox::warning(this,QStringLiteral("이펙트 오류"),QString::fromUtf8(e.what()));return;}doc_->undoStack()->push(new SetEffectsCommand(doc_,effects_));accept();});
        rebuild(0);
    }
private:
    static bool uses(const std::vector<naru::EventCommand>& commands,const std::string& id){for(const auto& c:commands){if(c.type==naru::EventCommandType::ShowEffect&&c.effect==id)return true;if(uses(c.thenCommands,id)||uses(c.elseCommands,id))return true;for(const auto& choice:c.choices)if(uses(choice.commands,id))return true;}return false;}
    void drawPreview(){if(current_<0||current_>=int(effects_.size())||effects_[current_].frames.empty())return;const auto& f=effects_[current_].frames[previewFrame_];auto image=loadProjectImage(doc_,f.image).copy(f.x,f.y,f.width,f.height);
        QPixmap pix(320,160);pix.fill(QColor("#30343c"));QPainter p(&pix);if(f.screenShake)p.translate(4,3);p.setPen(Qt::white);p.drawRect(145,90,30,55);if(!image.isNull()){p.save();p.translate(160+f.offsetX,95+f.offsetY);p.rotate(f.rotation);p.setOpacity(f.opacity);p.setCompositionMode(f.blend=="add"?QPainter::CompositionMode_Plus:QPainter::CompositionMode_SourceOver);p.drawImage(QRectF(-f.width*f.scale/2,-f.height*f.scale/2,f.width*f.scale,f.height*f.scale),image);p.restore();}if(f.targetFlash){p.fillRect(145,90,30,55,QColor(255,255,255,130));}if(f.screenFlash)p.fillRect(pix.rect(),QColor(255,255,255,85));p.end();preview_->setPixmap(pix);previewTimer_.setInterval(std::max(10,int(f.seconds*1000)));}
    void append(const naru::EffectFrame& f){int row=table_->rowCount();table_->insertRow(row);QStringList v{QString::fromStdString(f.image),QString::number(f.x),QString::number(f.y),QString::number(f.width),QString::number(f.height),QString::number(f.seconds),QString::number(f.offsetX),QString::number(f.offsetY),QString::number(f.scale),QString::number(f.rotation),QString::number(f.opacity),QString::fromStdString(f.blend),QString::fromStdString(f.sound),QString::number(int(f.targetFlash)),QString::number(int(f.screenFlash)),QString::number(int(f.screenShake))};for(int i=0;i<v.size();++i)table_->setItem(row,i,new QTableWidgetItem(v[i]));}
    void store(){if(current_<0||current_>=int(effects_.size()))return;auto& e=effects_[current_];e.id=id_->text().trimmed().toStdString();e.name=name_->text().trimmed().toStdString();e.frames.clear();for(int row=0;row<table_->rowCount();++row){auto value=[&](int c){return table_->item(row,c)?table_->item(row,c)->text():QString();};naru::EffectFrame f;f.image=value(0).toStdString();f.x=value(1).toInt();f.y=value(2).toInt();f.width=value(3).toInt();f.height=value(4).toInt();f.seconds=value(5).toDouble();f.offsetX=value(6).toDouble();f.offsetY=value(7).toDouble();f.scale=value(8).toDouble();f.rotation=value(9).toDouble();f.opacity=value(10).toDouble();f.blend=value(11).toStdString();f.sound=value(12).toStdString();f.targetFlash=value(13)=="1";f.screenFlash=value(14)=="1";f.screenShake=value(15)=="1";e.frames.push_back(f);}}
    void load(){table_->setRowCount(0);if(current_<0||current_>=int(effects_.size()))return;const auto& e=effects_[current_];id_->setText(QString::fromStdString(e.id));name_->setText(QString::fromStdString(e.name));for(const auto& f:e.frames)append(f);}
    void rebuild(int selected){list_->blockSignals(true);list_->clear();for(const auto& e:effects_)list_->addItem(QString::fromStdString(e.name+" ("+e.id+")"));list_->blockSignals(false);current_=-1;list_->setCurrentRow(effects_.empty()?-1:std::clamp(selected,0,int(effects_.size())-1));}
    ProjectDocument* doc_;std::vector<naru::Effect> effects_;int current_=-1;QListWidget* list_;QLineEdit *id_,*name_;QTableWidget* table_;QComboBox* image_;QSpinBox *width_,*height_;AnimationSheetView* sheet_;QLabel* preview_;QTimer previewTimer_;std::size_t previewFrame_=0;
};
