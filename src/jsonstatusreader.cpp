#include "jsonstatusreader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

JsonStatusReader::JsonStatusReader(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
{
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(100);
    connect(m_reloadTimer, &QTimer::timeout, this, &JsonStatusReader::reload);

    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &JsonStatusReader::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &JsonStatusReader::onDirectoryChanged);
}

QUrl JsonStatusReader::source() const
{
    return m_source;
}

void JsonStatusReader::setSource(const QUrl &source)
{
    if (m_source == source)
        return;

    m_source = source;
    emit sourceChanged();

    setupWatcher();
    reload();
}

QJsonObject JsonStatusReader::data() const
{
    return m_data;
}

QString JsonStatusReader::error() const
{
    return m_error;
}

bool JsonStatusReader::valid() const
{
    return m_valid;
}

void JsonStatusReader::reload()
{
    updateData();
}

void JsonStatusReader::onFileChanged(const QString &)
{
    // Debounce: if file is recreated rapidly (mv tmp final), wait a moment
    m_reloadTimer->start();
    // Also re-add file to watcher in case it was replaced
    setupWatcher();
}

void JsonStatusReader::onDirectoryChanged(const QString &)
{
    m_reloadTimer->start();
}

void JsonStatusReader::setupWatcher()
{
    if (!m_source.isLocalFile())
        return;

    const QString path = m_source.toLocalFile();
    const QFileInfo fi(path);

    QStringList toRemove = m_watcher->files() + m_watcher->directories();
    if (!toRemove.isEmpty())
        m_watcher->removePaths(toRemove);

    if (fi.exists())
        m_watcher->addPath(path);

    const QString dir = fi.absolutePath();
    if (QFile::exists(dir))
        m_watcher->addPath(dir);
}

void JsonStatusReader::updateData()
{
    if (!m_source.isLocalFile()) {
        if (m_valid) { m_valid = false; emit validChanged(); }
        if (m_error != QLatin1String("Source is not a local file")) {
            m_error = QStringLiteral("Source is not a local file");
            emit errorChanged();
        }
        return;
    }

    const QString path = m_source.toLocalFile();
    QFile file(path);

    if (!file.exists()) {
        if (m_valid) { m_valid = false; emit validChanged(); }
        if (m_error != QLatin1String("File not found")) {
            m_error = QStringLiteral("File not found");
            emit errorChanged();
        }
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (m_valid) { m_valid = false; emit validChanged(); }
        QString err = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        if (m_error != err) { m_error = err; emit errorChanged(); }
        return;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (m_valid) { m_valid = false; emit validChanged(); }
        QString err = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        if (m_error != err) { m_error = err; emit errorChanged(); }
        if (!m_data.isEmpty()) { m_data = QJsonObject(); emit dataChanged(); }
        return;
    }

    if (!doc.isObject()) {
        if (m_valid) { m_valid = false; emit validChanged(); }
        QString err = QStringLiteral("JSON root is not an object");
        if (m_error != err) { m_error = err; emit errorChanged(); }
        if (!m_data.isEmpty()) { m_data = QJsonObject(); emit dataChanged(); }
        return;
    }

    const QJsonObject newData = doc.object();
    if (m_data != newData) {
        m_data = newData;
        emit dataChanged();
    }

    if (!m_valid) {
        m_valid = true;
        emit validChanged();
    }
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
}
