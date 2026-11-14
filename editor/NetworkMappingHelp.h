#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

// An example of a server response, never the response from the current request.
class NetworkMappingHelp : public QDialog {
public:
    explicit NetworkMappingHelp(QWidget* parent = nullptr) : QDialog(parent) {
        setObjectName(QStringLiteral("networkMappingHelp"));
        setWindowTitle(QStringLiteral("응답 → 변수 설정 예시"));
        resize(620, 570);
        auto* layout = new QVBoxLayout(this);
        auto* intro = new QLabel(QStringLiteral("서버가 보낸 값 중 필요한 항목을 골라 게임 변수에 넣습니다.\n"
            "아래는 설명용 예시입니다. 실제 응답의 항목 이름은 서버마다 다릅니다."), this);
        intro->setWordWrap(true); intro->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(intro);
        auto* example = new QPlainTextEdit(this);
        example->setObjectName(QStringLiteral("networkMappingExample"));
        example->setReadOnly(true);
        example->setPlainText(QStringLiteral(
            "서버 응답 예시 (JSON)\n"
            "{\n    \"message\": \"안녕하세요\",\n    \"score\": 42,\n"
            "    \"notice\": { \"title\": \"점검 안내\" },\n"
            "    \"items\": [\"물약\", \"검\"]\n}\n\n"
            "응답 경로       넣을 변수         실행 후 변수 값\n"
            "/message        공지내용          안녕하세요\n"
            "/score          점수              42\n"
            "/notice/title   공지제목          점검 안내\n"
            "/items/0        첫아이템          물약\n\n"
            "1. API 요청 → 변수의 응답 → 변수에서 행 추가를 누릅니다.\n"
            "2. 왼쪽에 응답 경로 /message를 입력합니다.\n"
            "3. 오른쪽에 게임 → 변수·스위치에 만든 공지내용을 입력합니다.\n"
            "4. 요청 성공 후 실행 변수에서 공지내용을 확인합니다.\n\n"
            "경로의 이름은 서버 응답을 따릅니다. /items/0은 첫 항목입니다.\n"
            "서버에 없는 경로 또는 객체·배열 전체를 선택하면 mapping_error가 발생합니다.\n"
            "문자열·숫자·참/거짓·null 값만 변수에 넣을 수 있습니다.\n"
            "항목 이름의 /는 ~1, ~는 ~0으로 적습니다. 빈 경로는 응답 전체입니다.\n"
            "실제 응답 형식은 연결하는 서버의 API 안내에서 확인하세요.\n"
            "이 예시는 현재 요청을 보내거나 입력값을 변경하지 않습니다."));
        layout->addWidget(example, 1);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }
};
