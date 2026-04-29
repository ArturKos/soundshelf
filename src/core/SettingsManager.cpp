#include "soundshelf/core/SettingsManager.hpp"

#include <QSettings>
#include <QStandardPaths>

namespace soundshelf {

namespace {

QSettings& s() {
    static QSettings inst;
    return inst;
}

template<typename T>
T get(const QString& key, const T& def) {
    return s().value(key, def).template value<T>();
}

template<typename T>
void set(const QString& key, const T& value) {
    s().setValue(key, value);
}

} // anonymous

SettingsManager& SettingsManager::instance() {
    static SettingsManager mgr;
    return mgr;
}

QString SettingsManager::locale() const {
    return get<QString>(QStringLiteral("locale"), QString());
}
void SettingsManager::setLocale(const QString& code) {
    set(QStringLiteral("locale"), code);
    emit changed(QStringLiteral("locale"));
}

QString SettingsManager::theme() const {
    return get<QString>(QStringLiteral("theme"), QStringLiteral("modern_dark"));
}
void SettingsManager::setTheme(const QString& name) {
    set(QStringLiteral("theme"), name);
    emit changed(QStringLiteral("theme"));
}

QString SettingsManager::musicLibraryPath() const {
    const QString def = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return get<QString>(QStringLiteral("library/path"), def);
}
void SettingsManager::setMusicLibraryPath(const QString& path) {
    set(QStringLiteral("library/path"), path);
    emit changed(QStringLiteral("library/path"));
}

QStringList SettingsManager::watchedFolders() const {
    return get<QStringList>(QStringLiteral("library/watched_folders"), QStringList());
}
void SettingsManager::setWatchedFolders(const QStringList& list) {
    set(QStringLiteral("library/watched_folders"), list);
    emit changed(QStringLiteral("library/watched_folders"));
}

bool SettingsManager::autoUpdateOnFileChanges() const {
    return get<bool>(QStringLiteral("library/auto_update"), true);
}
void SettingsManager::setAutoUpdateOnFileChanges(bool b) {
    set(QStringLiteral("library/auto_update"), b);
    emit changed(QStringLiteral("library/auto_update"));
}

bool SettingsManager::resumePlaybackOnStart() const {
    return get<bool>(QStringLiteral("startup/resume_playback"), true);
}
void SettingsManager::setResumePlaybackOnStart(bool b) {
    set(QStringLiteral("startup/resume_playback"), b);
}

bool SettingsManager::startMinimizedToTray() const {
    return get<bool>(QStringLiteral("startup/minimized_to_tray"), false);
}
void SettingsManager::setStartMinimizedToTray(bool b) {
    set(QStringLiteral("startup/minimized_to_tray"), b);
}

QString SettingsManager::ripDefaultFormat() const {
    return get<QString>(QStringLiteral("disc/rip_format"), QStringLiteral("flac"));
}
void SettingsManager::setRipDefaultFormat(const QString& fmt) {
    set(QStringLiteral("disc/rip_format"), fmt);
}

QString SettingsManager::ripQuality() const {
    return get<QString>(QStringLiteral("disc/rip_quality"), QStringLiteral("VBR"));
}
void SettingsManager::setRipQuality(const QString& q) {
    set(QStringLiteral("disc/rip_quality"), q);
}

QString SettingsManager::fileNamingPattern() const {
    return get<QString>(QStringLiteral("disc/naming_pattern"),
                        QStringLiteral("{albumartist}/{album}/{track:02} - {title}"));
}
void SettingsManager::setFileNamingPattern(const QString& pattern) {
    set(QStringLiteral("disc/naming_pattern"), pattern);
}

bool SettingsManager::replayGainEnabled() const {
    return get<bool>(QStringLiteral("playback/replaygain"), true);
}
void SettingsManager::setReplayGainEnabled(bool b) {
    set(QStringLiteral("playback/replaygain"), b);
}

bool SettingsManager::replayGainAlbumMode() const {
    return get<bool>(QStringLiteral("playback/replaygain_album"), false);
}
void SettingsManager::setReplayGainAlbumMode(bool b) {
    set(QStringLiteral("playback/replaygain_album"), b);
}

bool SettingsManager::gaplessEnabled() const {
    return get<bool>(QStringLiteral("playback/gapless"), true);
}
void SettingsManager::setGaplessEnabled(bool b) {
    set(QStringLiteral("playback/gapless"), b);
}

int SettingsManager::crossfadeMs() const {
    return get<int>(QStringLiteral("playback/crossfade_ms"), 0);
}
void SettingsManager::setCrossfadeMs(int ms) {
    set(QStringLiteral("playback/crossfade_ms"), ms);
}

bool SettingsManager::acoustidEnabled() const {
    return get<bool>(QStringLiteral("library/acoustid"), false);
}
void SettingsManager::setAcoustidEnabled(bool b) {
    set(QStringLiteral("library/acoustid"), b);
}

QString SettingsManager::lastFmSessionToken() const {
    return get<QString>(QStringLiteral("scrobble/lastfm_token"), QString());
}
void SettingsManager::setLastFmSessionToken(const QString& tok) {
    set(QStringLiteral("scrobble/lastfm_token"), tok);
}

QString SettingsManager::listenBrainzToken() const {
    return get<QString>(QStringLiteral("scrobble/listenbrainz_token"), QString());
}
void SettingsManager::setListenBrainzToken(const QString& tok) {
    set(QStringLiteral("scrobble/listenbrainz_token"), tok);
}

QString SettingsManager::musicBrainzUserToken() const {
    return get<QString>(QStringLiteral("network/mb_user_token"), QString());
}
void SettingsManager::setMusicBrainzUserToken(const QString& tok) {
    set(QStringLiteral("network/mb_user_token"), tok);
}

} // namespace soundshelf
