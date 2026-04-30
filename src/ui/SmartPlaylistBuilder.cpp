#include "soundshelf/ui/SmartPlaylistBuilder.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QFontDatabase>

namespace soundshelf {

SmartPlaylistBuilder::SmartPlaylistBuilder(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Name:"), m_nameEdit);
    root->addLayout(form);

    root->addWidget(new QLabel(tr("Rules (JSON):"), this));
    m_jsonEdit = new QPlainTextEdit(this);
    m_jsonEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_jsonEdit->setPlaceholderText(QStringLiteral(
        "{\n  \"match\": \"all\",\n"
        "  \"rules\": [\n"
        "    { \"field\": \"genre\",  \"op\": \"is\", \"value\": \"Jazz\" },\n"
        "    { \"field\": \"rating\", \"op\": \">=\", \"value\": 4 }\n"
        "  ],\n  \"limit\": 50,\n  \"order_by\": \"random\"\n}"));
    root->addWidget(m_jsonEdit, 1);

    m_save = new QPushButton(tr("Save"), this);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_save);
    root->addLayout(row);

    connect(m_save, &QPushButton::clicked, this, [this]() {
        const QString name = m_nameEdit->text().trimmed();
        const QString json = m_jsonEdit->toPlainText();
        QJsonParseError err;
        QJsonDocument::fromJson(json.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            QMessageBox::warning(this, tr("Invalid JSON"),
                tr("Cannot parse rules JSON: %1").arg(err.errorString()));
            return;
        }
        if (name.isEmpty()) {
            QMessageBox::warning(this, tr("Missing name"),
                tr("Smart playlist needs a name."));
            return;
        }
        emit saveRequested(name, json);
    });
}

SmartPlaylistBuilder::~SmartPlaylistBuilder() = default;

void SmartPlaylistBuilder::setPlaylist(const QString& name, const QString& rulesJson) {
    m_nameEdit->setText(name);
    m_jsonEdit->setPlainText(rulesJson);
}

QString SmartPlaylistBuilder::name() const { return m_nameEdit->text(); }
QString SmartPlaylistBuilder::rulesJson() const { return m_jsonEdit->toPlainText(); }

} // namespace soundshelf
