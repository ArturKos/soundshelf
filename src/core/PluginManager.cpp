#include "soundshelf/core/PluginManager.hpp"
#include "soundshelf/plugins/WinampVisAdapter.hpp"
#include "soundshelf/plugins/NativeVisPlugin.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QPluginLoader>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPlug, "soundshelf.core.plugins")

namespace soundshelf {

PluginManager::PluginManager(QObject* parent) : QObject(parent) {
    m_searchPaths = defaultPluginPaths();
}

PluginManager::~PluginManager() {
    qDeleteAll(m_plugins);
}

QStringList PluginManager::defaultPluginPaths() {
    QStringList paths;
    const QString data = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    paths << QDir(data).filePath(QStringLiteral("plugins"));
    paths << QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
    return paths;
}

void PluginManager::addSearchPath(const QString& path) {
    if (!m_searchPaths.contains(path)) m_searchPaths.append(path);
}

Result<void> PluginManager::scan() {
    qDeleteAll(m_plugins);
    m_plugins.clear();
    m_active = nullptr;

    int found = 0;
    for (const QString& dir : m_searchPaths) {
        QDir d(dir);
        if (!d.exists()) continue;
        const QFileInfoList files = d.entryInfoList(
            { QStringLiteral("*.so"), QStringLiteral("*.dll"), QStringLiteral("*.dylib") },
            QDir::Files);
        for (const QFileInfo& fi : files) {
            const QString name = fi.completeBaseName();
            if (name.startsWith(QLatin1String("vis_"))) {
                auto* w = new WinampVisAdapter(this);
                if (auto r = w->load(fi.absoluteFilePath()); !r) {
                    qCWarning(lcPlug) << "Winamp plugin" << fi.fileName()
                                      << "failed:" << r.error().message;
                    delete w;
                    continue;
                }
                m_plugins.append(w);
                ++found;
                continue;
            }
            QPluginLoader loader(fi.absoluteFilePath());
            QObject* obj = loader.instance();
            if (!obj) {
                qCWarning(lcPlug) << "Cannot load" << fi.fileName()
                                  << ":" << loader.errorString();
                continue;
            }
            if (auto* p = qobject_cast<VisualizationPlugin*>(obj)) {
                p->setParent(this);
                m_plugins.append(p);
                ++found;
            }
        }
    }
    qCInfo(lcPlug) << "Discovered" << found << "plugins in" << m_searchPaths;
    emit pluginsChanged();
    return Result<void>::ok();
}

Result<void> PluginManager::setActive(const QString& id) {
    if (id.isEmpty()) {
        m_active = nullptr;
        emit activeChanged(nullptr);
        return Result<void>::ok();
    }
    for (auto* p : m_plugins) {
        if (p->id() == id) {
            m_active = p;
            emit activeChanged(m_active);
            return Result<void>::ok();
        }
    }
    return Result<void>::err(Error::InvalidArgument,
        QStringLiteral("plugin id %1 not found").arg(id));
}

} // namespace soundshelf
