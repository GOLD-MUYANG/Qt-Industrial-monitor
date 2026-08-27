#pragma once

#include "VisionTypes.h"

#include <QImage>
#include <QWidget>

class QLabel;
class QPushButton;
class QResizeEvent;

class VisionPage final : public QWidget
{
    Q_OBJECT

public:
    explicit VisionPage(QWidget *parent = nullptr);

public slots:
    void setSourceInfo(
        const industrial::monitor::vision::VisionSourceInfo &source);
    void setFrame(
        const industrial::monitor::vision::VisionFrameResult &frame);
    void setPlaybackState(
        industrial::monitor::vision::VisionPlaybackState state,
        const QString &message);
    void showError(const QString &message);

signals:
    void openVideoRequested(const QString &path);
    void playRequested();
    void pauseRequested();
    void stopRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void chooseVideo();

private:
    static QString defaultStateText(
        industrial::monitor::vision::VisionPlaybackState state);
    static QString formatDuration(qint64 milliseconds);
    void updateProgress(qint64 positionMs);
    void updateDisplayedFrame();

    QPushButton *m_selectButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_fileNameLabel = nullptr;
    QLabel *m_frameLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_progressLabel = nullptr;
    QLabel *m_targetLabel = nullptr;
    QLabel *m_detectionTimeLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QImage m_latestImage;
    industrial::monitor::vision::VisionSourceInfo m_source;
    industrial::monitor::vision::VisionPlaybackState m_state =
        industrial::monitor::vision::VisionPlaybackState::Idle;
};
