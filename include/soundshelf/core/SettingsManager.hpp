#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace soundshelf {

/// Zarządza ustawieniami aplikacji. Singleton.
/// Backend: QSettings (INI w ~/.config/soundshelf/) + DB settings table dla pewnych pól.
class SettingsManager : public QObject {
    Q_OBJECT
public:
    static SettingsManager& instance();

    // Common getters / setters
    QString locale() const;
    void setLocale(const QString& code);

    QString theme() const;
    void setTheme(const QString& name);

    QString musicLibraryPath() const;
    void setMusicLibraryPath(const QString& path);

    QStringList watchedFolders() const;
    void setWatchedFolders(const QStringList& list);

    bool autoUpdateOnFileChanges() const;
    void setAutoUpdateOnFileChanges(bool b);

    bool resumePlaybackOnStart() const;
    void setResumePlaybackOnStart(bool b);

    bool startMinimizedToTray() const;
    void setStartMinimizedToTray(bool b);

    QString ripDefaultFormat() const;
    void setRipDefaultFormat(const QString& fmt);

    QString ripQuality() const;
    void setRipQuality(const QString& q);

    QString fileNamingPattern() const;
    void setFileNamingPattern(const QString& pattern);

    bool replayGainEnabled() const;
    void setReplayGainEnabled(bool b);

    bool replayGainAlbumMode() const;
    void setReplayGainAlbumMode(bool b);

    bool gaplessEnabled() const;
    void setGaplessEnabled(bool b);

    int crossfadeMs() const;
    void setCrossfadeMs(int ms);

    bool acoustidEnabled() const;
    void setAcoustidEnabled(bool b);

    QString lastFmSessionToken() const;
    void setLastFmSessionToken(const QString& tok);

    QString listenBrainzToken() const;
    void setListenBrainzToken(const QString& tok);

    QString musicBrainzUserToken() const;
    void setMusicBrainzUserToken(const QString& tok);

signals:
    void changed(const QString& key);

private:
    SettingsManager() = default;
};

} // namespace soundshelf
