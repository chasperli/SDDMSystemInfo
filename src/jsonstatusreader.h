#ifndef JSONSTATUSREADER_H
#define JSONSTATUSREADER_H

#include <QObject>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QUrl>
#include <QTimer>

class JsonStatusReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QJsonObject data READ data NOTIFY dataChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY validChanged)

public:
    explicit JsonStatusReader(QObject *parent = nullptr);

    QUrl source() const;
    void setSource(const QUrl &source);

    QJsonObject data() const;
    QString error() const;
    bool valid() const;

public slots:
    void reload();

signals:
    void sourceChanged();
    void dataChanged();
    void errorChanged();
    void validChanged();

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);

private:
    void updateData();
    void setupWatcher();

    QFileSystemWatcher *m_watcher;
    QUrl m_source;
    QJsonObject m_data;
    QString m_error;
    bool m_valid = false;
    QTimer *m_reloadTimer;
};

#endif
