// src/cli/cli_main.cpp — entry point dla osobnego binarka soundshelf-cli

#include <QCoreApplication>
#include <QLoggingCategory>
#include "soundshelf/cli/CLIController.hpp"
#include "soundshelf/core/Translator.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SoundShelf CLI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("SoundShelf"));

    // Domyślnie nie spamujemy debugiem; -v zmienia to.
    QLoggingCategory::setFilterRules(QStringLiteral("soundshelf.*.debug=false"));

    soundshelf::CLIController ctrl;
    return ctrl.run(QCoreApplication::arguments());
}
