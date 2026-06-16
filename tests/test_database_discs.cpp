#include <QtTest>
#include <QTemporaryDir>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/core/Disc.hpp"

using namespace soundshelf;

class TestDatabaseDiscs : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    // IDs assigned by upsertDisc
    int m_folder1Id = -1;
    int m_folder2Id = -1;
    int m_physicalId = -1;
    int m_imageId    = -1;
    int m_remoteId   = -1;

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("discs_test.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        auto insert = [](const QString& title, DiscType type) -> int {
            Disc d;
            d.title = title;
            d.type  = type;
            // tocDiscId left empty → NULL → no ON CONFLICT hit → fresh row every time
            auto res = DatabaseManager::instance().upsertDisc(d);
            return res.isOk() ? res.value() : -1;
        };

        m_folder1Id  = insert(QStringLiteral("Folder Disc One"),  DiscType::Folder);
        m_folder2Id  = insert(QStringLiteral("Folder Disc Two"),  DiscType::Folder);
        m_physicalId = insert(QStringLiteral("Physical Disc"),    DiscType::Physical);
        m_imageId    = insert(QStringLiteral("Image Disc"),       DiscType::Image);
        m_remoteId   = insert(QStringLiteral("Remote Disc"),      DiscType::Remote);

        QVERIFY(m_folder1Id  > 0);
        QVERIFY(m_folder2Id  > 0);
        QVERIFY(m_physicalId > 0);
        QVERIFY(m_imageId    > 0);
        QVERIFY(m_remoteId   > 0);
    }

    void listFolderReturnsExactlyTwo() {
        auto r = DatabaseManager::instance().listDiscs(DiscType::Folder);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 2);
        for (const auto& d : r.value())
            QCOMPARE(d.type, DiscType::Folder);
    }

    void listPhysicalReturnsExactlyOne() {
        auto r = DatabaseManager::instance().listDiscs(DiscType::Physical);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 1);
        QCOMPARE(r.value()[0].id, m_physicalId);
        QCOMPARE(r.value()[0].type, DiscType::Physical);
    }

    void listImageReturnsExactlyOne() {
        auto r = DatabaseManager::instance().listDiscs(DiscType::Image);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 1);
        QCOMPARE(r.value()[0].id, m_imageId);
        QCOMPARE(r.value()[0].type, DiscType::Image);
    }

    void listRemoteReturnsExactlyOne() {
        auto r = DatabaseManager::instance().listDiscs(DiscType::Remote);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 1);
        QCOMPARE(r.value()[0].id, m_remoteId);
        QCOMPARE(r.value()[0].type, DiscType::Remote);
    }

    void mainWindowPatternNoDuplicates() {
        // Regression test for the triple-count bug: the 3-call concatenation
        // pattern used by MainWindow::reloadDiscs must yield 4 unique discs.
        auto rFolder   = DatabaseManager::instance().listDiscs(DiscType::Folder);
        auto rPhysical = DatabaseManager::instance().listDiscs(DiscType::Physical);
        auto rImage    = DatabaseManager::instance().listDiscs(DiscType::Image);

        QVERIFY(rFolder.isOk());
        QVERIFY(rPhysical.isOk());
        QVERIFY(rImage.isOk());

        QList<Disc> combined;
        combined << rFolder.value() << rPhysical.value() << rImage.value();

        QCOMPARE(combined.size(), 4);

        // Verify all IDs are distinct
        QSet<int> ids;
        for (const auto& d : combined)
            ids.insert(d.id);
        QCOMPARE(ids.size(), 4);
    }

    void limitArgumentIsHonored() {
        auto r = DatabaseManager::instance().listDiscs(DiscType::Folder, 1);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestDatabaseDiscs)
#include "test_database_discs.moc"
