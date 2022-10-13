#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include "Controller.h"
#include "DeviceMsg.h"
#include "Decoder.h"
#include "Device.h"
#include "FileHandler.h"
#include "Recorder.h"
#include "Server.h"
#include "Stream.h"

namespace qsc {

IDevice *IDevice::create(DeviceParams params, QObject *parent) {
    return new Device(params, parent);
}

Device::Device(DeviceParams params, QObject *parent)
    : IDevice(parent) {
    m_decoder = new Decoder(
        [this](int width,
               int height,
               uint8_t *dataY,
               uint8_t *dataU,
               uint8_t *dataV,
               int linesizeY,
               int linesizeU,
               int linesizeV) {
            for (const auto &item : m_deviceObservers) {
                item->onFrame(width, height, dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
            }
        },
        this);
    m_fileHandler = new FileHandler(this);
    m_controller = new Controller(
        [this](const QByteArray &buffer) -> qint64 {
            if (!m_server || !m_server->getControlSocket()) {
                return 0;
            }

            return m_server->getControlSocket()->write(buffer.data(), buffer.length());
        },
        params.gameScript,
        this);

    m_stream = new Stream(this);

    m_server = new Server(this);
    initSignals();
}

Device::~Device() {
    Device::disconnectDevice();
}

void Device::setUserData(void *data) {
    m_userData = data;
}

void *Device::getUserData() {
    return m_userData;
}

void Device::registerDeviceObserver(DeviceObserver *observer) {
    m_deviceObservers.insert(observer);
}

void Device::deRegisterDeviceObserver(DeviceObserver *observer) {
    m_deviceObservers.erase(observer);
}

const QString &Device::getSerial() {
    return m_params.serial;
}

void Device::updateScript(QString script) {
    if (m_controller) {
        m_controller->updateScript(script);
    }
}

void Device::screenshot() {
    if (!m_decoder) {
        return;
    }

    // screenshot
    m_decoder->peekFrame(
        [this](int width, int height, uint8_t *dataRGB32) { saveFrame(width, height, dataRGB32); });
}

void Device::showTouch(bool show) {
    AdbProcess *adb = new qsc::AdbProcess();
    if (!adb) {
        return;
    }
    connect(adb,
            &qsc::AdbProcess::adbProcessResult,
            this,
            [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
                if (AdbProcess::AER_SUCCESS_START != processResult) {
                    sender()->deleteLater();
                }
            });
    adb->setShowTouchesEnabled(getSerial(), show);

    qInfo() << getSerial() << " show touch " << (show ? "enable" : "disable");
}

void Device::initSignals() {
    if (m_controller) {
        connect(m_controller, &Controller::grabCursor, this, [this](bool grab) {
            for (const auto &item : m_deviceObservers) {
                item->grabCursor(grab);
            }
        });
    }
    if (m_fileHandler) {
        connect(m_fileHandler,
                &FileHandler::fileHandlerResult,
                this,
                [this](FileHandler::FILE_HANDLER_RESULT processResult, bool isApk) {
                    QString tipsType = "";
                    if (isApk) {
                        tipsType = "install apk";
                    } else {
                        tipsType = "file transfer";
                    }
                    QString tips;
                    if (FileHandler::FAR_IS_RUNNING == processResult) {
                        tips = QString("wait current %1 to complete").arg(tipsType);
                    }
                    if (FileHandler::FAR_SUCCESS_EXEC == processResult) {
                        tips = QString("%1 complete, save in %2")
                                   .arg(tipsType)
                                   .arg(m_params.pushFilePath);
                    }
                    if (FileHandler::FAR_ERROR_EXEC == processResult) {
                        tips = QString("%1 failed").arg(tipsType);
                    }
                    qInfo() << tips;
                });
    }

    if (m_server) {
        connect(
            m_server,
            &Server::serverStarted,
            this,
            [this](bool success, const QString &deviceName, const QSize &size) {
                m_serverStartSuccess = success;
                emit deviceConnected(success, m_params.serial, deviceName, size);
                if (success) {
                    double diff = m_startTimeCount.elapsed() / 1000.0;
                    qInfo()
                        << QString("server start finish in %1s").arg(diff).toStdString().c_str();

                    // init recorder
                    if (m_recorder) {
                        m_recorder->setFrameSize(size);
                        if (!m_recorder->open()) {
                            qCritical("Could not open recorder");
                        }

                        if (!m_recorder->startRecorder()) {
                            qCritical("Could not start recorder");
                        }
                    }

                    // init decoder
                    if (m_decoder) {
                        m_decoder->open();
                    }

                    // init stream
                    m_stream->installVideoSocket(m_server->removeVideoSocket());
                    m_stream->startDecode();

                    // recv device msg
                    connect(m_server->getControlSocket(), &QTcpSocket::readyRead, this, [this]() {
                        if (!m_controller) {
                            return;
                        }

                        auto controlSocket = m_server->getControlSocket();
                        while (controlSocket->bytesAvailable()) {
                            QByteArray byteArray = controlSocket->peek(
                                controlSocket->bytesAvailable());
                            DeviceMsg deviceMsg;
                            qint32 consume = deviceMsg.deserialize(byteArray);
                            if (0 >= consume) {
                                break;
                            }
                            controlSocket->read(consume);
                            m_controller->recvDeviceMsg(&deviceMsg);
                        }
                    });

                    // 显示界面时才自动息屏（m_params.display）
                    if (m_params.closeScreen && m_controller) {
                        m_controller->setScreenPowerMode(ControlMsg::SPM_OFF);
                    }
                } else {
                    m_server->stop();
                }
            });
        connect(m_server, &Server::serverStoped, this, [this]() {
            disconnectDevice();
            qDebug() << "server process stop";
        });
    }

    if (m_stream) {
        connect(m_stream, &Stream::onStreamStop, this, [this]() {
            disconnectDevice();
            qDebug() << "stream thread stop";
        });
        connect(
            m_stream,
            &Stream::getFrame,
            this,
            [this](AVPacket *packet) {
                if (m_decoder && !m_decoder->push(packet)) {
                    qCritical("Could not send packet to decoder");
                }

                if (m_recorder && !m_recorder->push(packet)) {
                    qCritical("Could not send packet to recorder");
                }
            },
            Qt::DirectConnection);
        connect(
            m_stream,
            &Stream::getConfigFrame,
            this,
            [this](AVPacket *packet) {
                if (m_recorder && !m_recorder->push(packet)) {
                    qCritical("Could not send config packet to recorder");
                }
            },
            Qt::DirectConnection);
    }

    if (m_decoder) {
        connect(m_decoder, &Decoder::updateFPS, this, [this](quint32 fps) {
            for (const auto &item : m_deviceObservers) {
                item->updateFPS(fps);
            }
        });
    }
}

bool Device::startListen() {
    if (!m_server || m_serverStartSuccess) {
        return false;
    }

    m_startTimeCount.start();
    // max size support 480p 720p 1080p 设备原生分辨率
    // support wireless connect, example:
    // m_server->start("192.168.0.174:5555", 27183, m_maxSize, m_bitRate, "");
    // only one devices, serial can be null
    // mark: crop input format: "width:height:x:y" or "" for no crop, for example: "100:200:0:0"

    return m_server->start();
}

uint16_t Device::getPort() {
    return m_server->getPort();
}

void Device::disconnectDevice() {
    if (!m_server) {
        return;
    }
    m_server->stop();
    m_server = Q_NULLPTR;

    if (m_stream) {
        m_stream->stopDecode();
    }

    // server must stop before decoder, because decoder block main thread
    if (m_decoder) {
        m_decoder->close();
    }

    if (m_recorder) {
        if (m_recorder->isRunning()) {
            m_recorder->stopRecorder();
            m_recorder->wait();
        }
        m_recorder->close();
    }

    if (m_serverStartSuccess) {
        emit deviceDisconnected(m_params.serial);
    }
    m_serverStartSuccess = false;
}

void Device::postGoBack() {
    if (!m_controller) {
        return;
    }
    m_controller->postGoBack();
}

void Device::postGoHome() {
    if (!m_controller) {
        return;
    }
    m_controller->postGoHome();
}

void Device::postGoMenu() {
    if (!m_controller) {
        return;
    }
    m_controller->postGoMenu();
}

void Device::postAppSwitch() {
    if (!m_controller) {
        return;
    }
    m_controller->postAppSwitch();
}

void Device::postPower() {
    if (!m_controller) {
        return;
    }
    m_controller->postPower();
}

void Device::postVolumeUp() {
    if (!m_controller) {
        return;
    }
    m_controller->postVolumeUp();
}

void Device::postVolumeDown() {
    if (!m_controller) {
        return;
    }
    m_controller->postVolumeDown();
}

void Device::postCopy() {
    if (!m_controller) {
        return;
    }
    m_controller->copy();
}

void Device::postCut() {
    if (!m_controller) {
        return;
    }
    m_controller->cut();
}

void Device::setScreenPowerMode(bool open) {
    if (!m_controller) {
        return;
    }
    ControlMsg::ScreenPowerMode mode{};
    if (open) {
        mode = ControlMsg::SPM_NORMAL;
    } else {
        mode = ControlMsg::SPM_OFF;
    }
    m_controller->setScreenPowerMode(mode);
}

void Device::expandNotificationPanel() {
    if (!m_controller) {
        return;
    }
    m_controller->expandNotificationPanel();
}

void Device::collapsePanel() {
    if (!m_controller) {
        return;
    }
    m_controller->collapsePanel();
}

void Device::postBackOrScreenOn(bool down) {
    if (!m_controller) {
        return;
    }
    m_controller->postBackOrScreenOn(down);
}

void Device::postTextInput(QString &text) {
    if (!m_controller) {
        return;
    }
    m_controller->postTextInput(text);
}

void Device::requestDeviceClipboard() {
    if (!m_controller) {
        return;
    }
    m_controller->requestDeviceClipboard();
}

void Device::setDeviceClipboard(bool pause) {
    if (!m_controller) {
        return;
    }
    m_controller->setDeviceClipboard(pause);
}

void Device::clipboardPaste() {
    if (!m_controller) {
        return;
    }
    m_controller->clipboardPaste();
}

void Device::pushFileRequest(const QString &file, const QString &devicePath) {
    if (!m_fileHandler) {
        return;
    }
    m_fileHandler->onPushFileRequest(getSerial(), file, devicePath);
}

void Device::installApkRequest(const QString &apkFile) {
    if (!m_fileHandler) {
        return;
    }
    m_fileHandler->onInstallApkRequest(getSerial(), apkFile);
}

void Device::mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize) {
    if (!m_controller) {
        return;
    }
    m_controller->mouseEvent(from, frameSize, showSize);
}

void Device::wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize) {
    if (!m_controller) {
        return;
    }
    m_controller->wheelEvent(from, frameSize, showSize);
}

void Device::keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize) {
    if (!m_controller) {
        return;
    }
    m_controller->keyEvent(from, frameSize, showSize);
}

bool Device::isCurrentCustomKeymap() {
    if (!m_controller) {
        return false;
    }
    return m_controller->isCurrentCustomKeymap();
}

bool Device::saveFrame(int width, int height, uint8_t *dataRGB32) {
    if (!dataRGB32) {
        return false;
    }

    QImage rgbImage(dataRGB32, width, height, QImage::Format_RGB32);

    // save
    QString absFilePath;
    QString fileDir(m_params.recordPath);
    if (fileDir.isEmpty()) {
        qWarning() << "please select record save path!!!";
        return false;
    }
    QDateTime dateTime = QDateTime::currentDateTime();
    QString fileName = dateTime.toString("_yyyyMMdd_hhmmss_zzz");
    fileName = m_params.serial + fileName + ".png";
    QDir dir(fileDir);
    absFilePath = dir.absoluteFilePath(fileName);
    int ret = rgbImage.save(absFilePath, "PNG", 100);
    if (!ret) {
        return false;
    }

    qInfo() << "screenshot save to " << absFilePath;
    return true;
}

} // namespace qsc