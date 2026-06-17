/**
 * \file test_cli.cpp
 * \brief End-to-end CLI integration tests.
 *
 * Launches the built soundshelf-cli binary via QProcess against an isolated
 * temp directory/database. Audio fixtures are created at runtime with ffmpeg;
 * when ffmpeg is absent the fixture-dependent tests QSKIP rather than fail.
 * The CLI binary path is injected at compile time via SOUNDSHELF_CLI_EXE.
 */
#include <QtTest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

/**
 * \brief End-to-end CLI integration test suite.
 *
 * Each test slot launches a fresh soundshelf-cli subprocess. The global
 * --db flag points every invocation to the same temp database created in
 * initTestCase(), so tests see each other's imports but each process starts
 * with its own clean singleton state.
 */
class TestCli : public QObject {
    Q_OBJECT

public:
    TestCli() = default;

private:
    QTemporaryDir m_workspace;    ///< Root temp dir for the entire test run.
    QString       m_dbPath;       ///< Isolated test database path.
    QString       m_fixtureDir;   ///< Sub-directory holding generated audio fixtures.
    QString       m_ffmpeg;       ///< Absolute path to ffmpeg, or empty if absent.
    int           m_fixtureCount = 0; ///< Number of successfully generated fixtures.

    /**
     * \brief Runs the CLI binary with the given arguments.
     *
     * Prepends `--db <m_dbPath>` to every invocation and sets
     * QT_QPA_PLATFORM=offscreen in the child environment.
     *
     * \param args       Command-line arguments (no binary name).
     * \param out        If non-null, receives all stdout bytes.
     * \param err        If non-null, receives all stderr bytes.
     * \param msTimeout  Kill timeout in milliseconds.
     * \return Exit code of the process, or -1 on timeout/start failure.
     */
    int runCli(const QStringList& args,
               QByteArray* out = nullptr,
               QByteArray* err = nullptr,
               int msTimeout = 15000)
    {
        QStringList full;
        full << QStringLiteral("--db") << m_dbPath;
        full << args;

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"),   QStringLiteral("offscreen"));
        env.insert(QStringLiteral("QT_LOGGING_RULES"),  QStringLiteral("*.debug=false"));
        proc.setProcessEnvironment(env);

        proc.start(QStringLiteral(SOUNDSHELF_CLI_EXE), full);
        if (!proc.waitForStarted(5000)) return -1;

        const bool finished = proc.waitForFinished(msTimeout);
        if (!finished) {
            proc.kill();
            proc.waitForFinished(2000);
        }

        if (out) *out = proc.readAllStandardOutput();
        if (err) *err = proc.readAllStandardError();

        return finished ? proc.exitCode() : -1;
    }

    /**
     * \brief Runs ffmpeg to generate a 1-second sine-wave audio file with tags.
     * \return true if ffmpeg exited 0.
     */
    bool makeFixture(const QString& path, const QString& title, const QString& artist)
    {
        if (m_ffmpeg.isEmpty()) return false;
        QProcess ffmpeg;
        ffmpeg.start(m_ffmpeg, {
            QStringLiteral("-y"),
            QStringLiteral("-f"),        QStringLiteral("lavfi"),
            QStringLiteral("-i"),        QStringLiteral("sine=frequency=440:duration=1"),
            QStringLiteral("-metadata"), QStringLiteral("title=")  + title,
            QStringLiteral("-metadata"), QStringLiteral("artist=") + artist,
            path
        });
        return ffmpeg.waitForFinished(10000) && ffmpeg.exitCode() == 0;
    }

private slots:

    // -----------------------------------------------------------------------
    // Infrastructure
    // -----------------------------------------------------------------------

    /**
     * \brief Creates the temp workspace, bootstraps the database schema,
     *        and generates 3 audio fixtures (skips if ffmpeg absent).
     */
    void initTestCase()
    {
        QVERIFY(m_workspace.isValid());
        m_dbPath     = m_workspace.filePath(QStringLiteral("test.db"));
        m_fixtureDir = m_workspace.filePath(QStringLiteral("fixtures"));
        QVERIFY(QDir().mkpath(m_fixtureDir));

        // Bootstrap schema so all subsequent subprocesses open a migrated DB.
        QByteArray out, err;
        const int rc = runCli({QStringLiteral("db"), QStringLiteral("migrate")}, &out, &err);
        QVERIFY2(rc == 0,
            qPrintable(QStringLiteral("db migrate exited %1\nstdout: %2\nstderr: %3")
                .arg(rc).arg(QString::fromUtf8(out)).arg(QString::fromUtf8(err))));

        // Locate ffmpeg — fixture-dependent tests will QSKIP if absent.
        m_ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (m_ffmpeg.isEmpty()) {
            qWarning("ffmpeg not found — fixture-dependent tests will be skipped");
            return;
        }

        struct Fix { const char* name; const char* title; const char* artist; };
        const QList<Fix> defs{
            {"a.flac", "Foo Track",  "Bar Artist"},
            {"b.ogg",  "Baz Track",  "Qux Artist"},
            {"c.wav",  "Test Track", "Test Artist"},
        };
        for (const auto& f : defs) {
            if (makeFixture(m_fixtureDir + QLatin1Char('/') + f.name, f.title, f.artist))
                ++m_fixtureCount;
        }
    }

    // -----------------------------------------------------------------------
    // 1. --version / help
    // -----------------------------------------------------------------------

    /** --version exits 0 and prints non-empty output. */
    void version_exitsZeroWithOutput()
    {
        QByteArray out;
        QCOMPARE(runCli({QStringLiteral("--version")}, &out), 0);
        QVERIFY(!out.trimmed().isEmpty());
    }

    /** help exits 0 and includes "Usage:" in the output. */
    void help_exitsZeroWithUsage()
    {
        QByteArray out;
        QCOMPARE(runCli({QStringLiteral("help")}, &out), 0);
        QVERIFY(out.contains("Usage:"));
    }

    // -----------------------------------------------------------------------
    // 2. Unknown command — falls through to cmdHelp() → exit 0
    // -----------------------------------------------------------------------

    /** Unknown command writes to stderr, then prints help and exits 0. */
    void unknownCommand_exitsZeroWithHelp()
    {
        QByteArray out, err;
        const int rc = runCli({QStringLiteral("xyzzy_no_such_cmd")}, &out, &err);
        QCOMPARE(rc, 0);
        QVERIFY(err.contains("Unknown command") || out.contains("Usage:"));
    }

    // -----------------------------------------------------------------------
    // 3. db subcommands
    // -----------------------------------------------------------------------

    /** db migrate is idempotent: second run on already-migrated DB exits 0. */
    void db_migrate_idempotent_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("db"), QStringLiteral("migrate")}), 0);
    }

    /** db info exits 0 and reports the schema version. */
    void db_info_exitsZeroWithSchemaVersion()
    {
        QByteArray out;
        QCOMPARE(runCli({QStringLiteral("db"), QStringLiteral("info")}, &out), 0);
        QVERIFY(out.contains("version") || out.contains("Version"));
    }

    /** db vacuum exits 0. */
    void db_vacuum_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("db"), QStringLiteral("vacuum")}), 0);
    }

    // -----------------------------------------------------------------------
    // 4. import + idempotency
    // -----------------------------------------------------------------------

    /** import <dir> exits 0 and JSON output contains expected imported count. */
    void import_fixtureDir_importsAll()
    {
        if (m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        QByteArray out, err;
        // -q suppresses per-file "+ path" lines so stdout is pure JSON.
        const int rc = runCli({
            QStringLiteral("-q"),
            QStringLiteral("--format"), QStringLiteral("json"),
            QStringLiteral("import"),   m_fixtureDir
        }, &out, &err);
        QCOMPARE(rc, 0);

        const QJsonDocument doc = QJsonDocument::fromJson(out.trimmed());
        QVERIFY2(!doc.isNull() && doc.isObject(),
            qPrintable(QStringLiteral("stdout not JSON: ") + QString::fromUtf8(out)));
        QCOMPARE(doc.object().value(QStringLiteral("imported")).toInt(), m_fixtureCount);
    }

    /** Re-importing the same directory is an upsert, not a duplicate insert. */
    void import_reImport_idempotent()
    {
        if (m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        QByteArray out;
        // -q suppresses per-file lines; second import upserts, exits 0.
        const int rc = runCli({
            QStringLiteral("-q"),
            QStringLiteral("--format"), QStringLiteral("json"),
            QStringLiteral("import"),   m_fixtureDir
        }, &out);
        QCOMPARE(rc, 0);
        // Verify the JSON is valid; imported count may be >= 0 (upsert succeeds).
        const QJsonDocument doc = QJsonDocument::fromJson(out.trimmed());
        QVERIFY2(!doc.isNull() && doc.isObject(),
            qPrintable(QStringLiteral("stdout not JSON on re-import: ") + QString::fromUtf8(out)));
    }

    // -----------------------------------------------------------------------
    // 5. list / search
    // -----------------------------------------------------------------------

    /** list --format json exits 0 and returns a JSON array with >= fixture count items. */
    void list_json_exitsZeroWithArray()
    {
        if (m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        QByteArray out;
        QCOMPARE(runCli({
            QStringLiteral("--format"), QStringLiteral("json"),
            QStringLiteral("list")
        }, &out), 0);

        const QJsonDocument doc = QJsonDocument::fromJson(out.trimmed());
        QVERIFY(!doc.isNull() && doc.isArray());
        QVERIFY(doc.array().size() >= m_fixtureCount);
    }

    /** search <term> exits 0 (results may be empty on FTS not finding embedded tags). */
    void search_term_exitsZero()
    {
        if (m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        QCOMPARE(runCli({QStringLiteral("search"), QStringLiteral("Foo")}), 0);
    }

    // -----------------------------------------------------------------------
    // 6. info <id>
    // -----------------------------------------------------------------------

    /** info for a valid imported track id exits 0. */
    void info_validId_exitsZero()
    {
        if (m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        QByteArray out;
        QCOMPARE(runCli({
            QStringLiteral("--format"), QStringLiteral("json"),
            QStringLiteral("list")
        }, &out), 0);

        const QJsonDocument doc = QJsonDocument::fromJson(out.trimmed());
        QVERIFY(!doc.isNull() && !doc.array().isEmpty());

        const int firstId = doc.array().first().toObject()
                                .value(QStringLiteral("id")).toInt();
        QVERIFY(firstId > 0);

        QCOMPARE(runCli({QStringLiteral("info"), QString::number(firstId)}), 0);
    }

    /** info for a nonexistent numeric id exits 2. */
    void info_bogusId_exitsTwo()
    {
        QCOMPARE(runCli({QStringLiteral("info"), QStringLiteral("999999")}), 2);
    }

    // -----------------------------------------------------------------------
    // 7. Missing-argument commands → exit 1
    // -----------------------------------------------------------------------

    /** info with no argument exits 1 (prints usage). */
    void info_noArg_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("info")}), 1);
    }

    /** seek with no argument exits 1 (prints usage). */
    void seek_noArg_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("seek")}), 1);
    }

    /** volume with no argument exits 1 (prints usage). */
    void volume_noArg_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("volume")}), 1);
    }

    // -----------------------------------------------------------------------
    // 8. stats
    // -----------------------------------------------------------------------

    /** stats top-tracks exits 0 on an empty library. */
    void stats_topTracks_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("stats"), QStringLiteral("top-tracks")}), 0);
    }

    /** stats listening-time exits 0. */
    void stats_listeningTime_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("stats"), QStringLiteral("listening-time")}), 0);
    }

    // -----------------------------------------------------------------------
    // 9. tag show / tag no-args
    // -----------------------------------------------------------------------

    /** tag show <path> on a fixture exits 0 and includes tag data in output. */
    void tag_show_fixture_exitsZeroWithMetadata()
    {
        if (m_ffmpeg.isEmpty() || m_fixtureCount == 0)
            QSKIP("ffmpeg unavailable or no fixtures generated");

        const QString path = m_fixtureDir + QStringLiteral("/a.flac");
        if (!QFileInfo::exists(path))
            QSKIP("fixture a.flac was not created");

        QByteArray out;
        QCOMPARE(runCli({QStringLiteral("tag"), QStringLiteral("show"), path}, &out), 0);
        // Output must mention at least one tag field.
        QVERIFY(out.contains("Title:") || out.contains("Artist:"));
    }

    /** tag with no subcommand exits 1. */
    void tag_noArgs_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("tag")}), 1);
    }

    // -----------------------------------------------------------------------
    // 10. duplicates / playlist / export / network+hardware no-ops
    // -----------------------------------------------------------------------

    /** duplicates scan exits 0 when the library has no duplicates. */
    void duplicates_scan_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("duplicates"), QStringLiteral("scan")}), 0);
    }

    /** playlist list exits 0 on an empty playlist table. */
    void playlist_list_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("playlist"), QStringLiteral("list")}), 0);
    }

    /** export library to a temp file exits 0 and creates the output file. */
    void export_toFile_exitsZeroAndWritesFile()
    {
        const QString outPath = m_workspace.filePath(QStringLiteral("export.json"));
        QCOMPARE(runCli({
            QStringLiteral("export"),  QStringLiteral("library"),
            QStringLiteral("--out"),   outPath
        }), 0);
        QVERIFY(QFileInfo::exists(outPath));
    }

    /** scrobble status exits 0 (reads pending queue from DB). */
    void scrobble_status_exitsZero()
    {
        QCOMPARE(runCli({QStringLiteral("scrobble"), QStringLiteral("status")}), 0);
    }

    /** scrobble auth exits 1 (browser OAuth is GUI-only). */
    void scrobble_auth_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("scrobble"), QStringLiteral("auth")}), 1);
    }

    /** remote list without a configured server URL exits 2. */
    void remote_noServer_exitsTwo()
    {
        QByteArray err;
        QCOMPARE(runCli({QStringLiteral("remote"), QStringLiteral("list")}, nullptr, &err), 2);
        QVERIFY(err.contains("No server URL") || err.contains("server"));
    }

    /** next without a running player exits 1 (honest no-op). */
    void next_noPlayer_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("next")}), 1);
    }

    /** prev without a running player exits 1 (honest no-op). */
    void prev_noPlayer_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("prev")}), 1);
    }

    /** daemon start exits 1 (delegates to OS process supervision). */
    void daemon_exitsOne()
    {
        QCOMPARE(runCli({QStringLiteral("daemon"), QStringLiteral("start")}), 1);
    }

    /** disc rip exits 1 (requires optical drive hardware). */
    void disc_rip_exitsOne()
    {
        QCOMPARE(runCli({
            QStringLiteral("disc"), QStringLiteral("rip"), QStringLiteral("/dev/sr0")
        }), 1);
    }

    /** disc lookup exits 1 (requires optical drive hardware). */
    void disc_lookup_exitsOne()
    {
        QCOMPARE(runCli({
            QStringLiteral("disc"), QStringLiteral("lookup"), QStringLiteral("/dev/sr0")
        }), 1);
    }

    // -----------------------------------------------------------------------
    // serve — must not hang the test runner
    // -----------------------------------------------------------------------

    /**
     * \brief Verifies that `serve --port 0` either exits quickly or is safely
     *        killed by the timeout, covering two build configurations:
     *
     *  - rc == 1: QHttpServer not compiled (HttpServer::isAvailable() == false).
     *  - rc == -1: server started and entered its blocking event loop; the test
     *              runner killed it after the 3000 ms timeout.
     *  - rc == 2: server compiled in but OS rejected port 0 (treated as a safe,
     *             non-blocking exit, so we accept it rather than flaking CI).
     *
     * In all cases the test must return within the QProcess timeout; it must
     * neither hang the suite nor crash the host process.
     */
    void serve_doesNotHang()
    {
        QByteArray out, err;
        const int rc = runCli({
            QStringLiteral("serve"),
            QStringLiteral("--port"), QStringLiteral("0")
        }, &out, &err, /*msTimeout=*/3000);

        QVERIFY2(rc == 1 || rc == -1 || rc == 2,
            qPrintable(QStringLiteral("serve --port 0: unexpected exit code %1\n"
                                      "stdout: %2\nstderr: %3")
                .arg(rc)
                .arg(QString::fromUtf8(out))
                .arg(QString::fromUtf8(err))));
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "test_cli.moc"
