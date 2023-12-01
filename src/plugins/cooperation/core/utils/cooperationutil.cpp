// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cooperationutil.h"
#include "cooperationutil_p.h"
#include "gui/mainwindow.h"
#include "maincontroller/maincontroller.h"
#include "transfer/transferhelper.h"
#include "cooperation/cooperationmanager.h"

#include "config/configmanager.h"
#include "common/constant.h"
#include "common/commonstruct.h"
#include "ipc/frontendservice.h"
#include "ipc/proto/comstruct.h"
#include "ipc/proto/backend.h"

#include <QJsonDocument>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QDebug>
#include <QDir>

using namespace cooperation_core;

CooperationUtilPrivate::CooperationUtilPrivate(CooperationUtil *qq)
    : q(qq)
{
    localIPCStart();

    UNIGO([this] {
        backendOk = pingBackend();
        LOG << "The result of ping backend is " << backendOk;
    });
}

CooperationUtilPrivate::~CooperationUtilPrivate()
{
}

bool CooperationUtilPrivate::pingBackend()
{
    rpc::Client rpcClient("127.0.0.1", UNI_IPC_BACKEND_PORT, false);
    co::Json req, res;
    bool onlyTransfer = qApp->property("onlyTransfer").toBool();
    int port = onlyTransfer
            ? UNI_IPC_FRONTEND_TRANSFER_PORT
            : UNI_IPC_FRONTEND_COOPERATION_PORT;

    ipc::PingBackParam backParam;
    backParam.who = qApp->applicationName().toStdString();
    backParam.version = fastring(UNI_IPC_PROTO);
    backParam.cb_port = port;

    req = backParam.as_json();
    req.add_member("api", "Backend.ping");   //BackendImpl::ping

    rpcClient.call(req, res);
    rpcClient.close();
    sessionId = res.get("msg").as_string().c_str();   // save the return session.

    //CallResult
    return res.get("result").as_bool() && !sessionId.isEmpty();
}

void CooperationUtilPrivate::localIPCStart()
{
    if (frontendIpcSer) return;

    frontendIpcSer = new FrontendService(this);

    UNIGO([this]() {
        while (!thisDestruct) {
            BridgeJsonData bridge;
            frontendIpcSer->bridgeChan()->operator>>(bridge);   //300ms超时
            if (!frontendIpcSer->bridgeChan()->done()) {
                // timeout, next read
                continue;
            }

            co::Json json_obj = json::parse(bridge.json);
            if (json_obj.is_null()) {
                WLOG << "parse error from: " << bridge.json.c_str();
                continue;
            }

            switch (bridge.type) {
            case IPC_PING: {
                ipc::PingFrontParam param;
                param.from_json(json_obj);

                bool result = false;
                fastring my_ver(FRONTEND_PROTO_VERSION);
                // test ping 服务测试用

                if (my_ver.compare(param.version) == 0 && (param.session.compare(sessionId.toStdString()) == 0 || param.session.compare("backendServerOnline") == 0)) {
                    result = true;
                } else {
                    WLOG << param.version.c_str() << " =version not match= " << my_ver.c_str();
                }

                BridgeJsonData res;
                res.type = IPC_PING;
                res.json = result ? param.session : "";   // 成功则返回session，否则为空

                frontendIpcSer->bridgeResult()->operator<<(res);
            } break;
            case FRONT_PEER_CB: {
                ipc::GenericResult param;
                param.from_json(json_obj);

                LOG << param.result << " peer : " << param.msg.c_str();

                co::Json obj = json::parse(param.msg);
                NodeInfo nodeInfo;
                nodeInfo.from_json(obj);
                if (nodeInfo.apps.empty() && !param.result) {
                    q->metaObject()->invokeMethod(MainController::instance(),
                                                  "updateDeviceList",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(QString, QString(nodeInfo.os.ipv4.c_str())),
                                                  Q_ARG(QString, QString("")),
                                                  Q_ARG(bool, param.result));
                    break;
                }

                for (const auto &appInfo : nodeInfo.apps) {
                    // 上线，非跨端应用无需处理
                    if (param.result && appInfo.appname.compare(CooperRegisterName) != 0)
                        continue;

                    // 下线，跨端应用未下线
                    if (!param.result && appInfo.appname.compare(CooperRegisterName) == 0)
                        continue;

                    q->metaObject()->invokeMethod(MainController::instance(),
                                                  "updateDeviceList",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(QString, QString(nodeInfo.os.ipv4.c_str())),
                                                  Q_ARG(QString, QString(appInfo.json.c_str())),
                                                  Q_ARG(bool, param.result));
                }
            } break;
            case FRONT_CONNECT_CB: {
                ipc::GenericResult param;
                param.from_json(json_obj);
                QString msg(param.msg.c_str());

                q->metaObject()->invokeMethod(TransferHelper::instance(), "onConnectStatusChanged",
                                              Qt::QueuedConnection,
                                              Q_ARG(int, param.result),
                                              Q_ARG(QString, msg),
                                              Q_ARG(bool, param.isself));
            } break;
            case FRONT_TRANS_STATUS_CB: {
                ipc::GenericResult param;
                param.from_json(json_obj);
                QString msg(param.msg.c_str());   // job path

                q->metaObject()->invokeMethod(TransferHelper::instance(), "onTransJobStatusChanged",
                                              Qt::QueuedConnection,
                                              Q_ARG(int, param.id),
                                              Q_ARG(int, param.result),
                                              Q_ARG(QString, msg));
            } break;
            case FRONT_NOTIFY_FILE_STATUS: {
                QString objstr(bridge.json.c_str());
                q->metaObject()->invokeMethod(TransferHelper::instance(),
                                              "onFileTransStatusChanged",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, objstr));
            } break;
            case FRONT_APPLY_TRANS_FILE: {
                ApplyTransFiles transferInfo;
                transferInfo.from_json(json_obj);
                LOG << "apply transfer info: " << json_obj;

                switch (transferInfo.type) {
                case ApplyTransType::APPLY_TRANS_CONFIRM:
                    q->metaObject()->invokeMethod(TransferHelper::instance(),
                                                  "accepted",
                                                  Qt::QueuedConnection);
                    break;
                case ApplyTransType::APPLY_TRANS_REFUSED:
                    q->metaObject()->invokeMethod(TransferHelper::instance(),
                                                  "rejected",
                                                  Qt::QueuedConnection);
                    break;
                default:
                    break;
                }
            } break;
            case FRONT_SERVER_ONLINE:
                backendOk = pingBackend();
                q->metaObject()->invokeMethod(MainController::instance(),
                                              "start",
                                              Qt::QueuedConnection);
                break;
            case FRONT_SHARE_APPLY_CONNECT: {
                ShareConnectApply conApply;
                conApply.from_json(json_obj);
                q->metaObject()->invokeMethod(CooperationManager::instance(),
                                              "notifyConnectRequest",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, QString(conApply.data.c_str())));
            } break;
            case FRONT_SHARE_APPLY_CONNECT_REPLY: {
                ShareConnectReply conReply;
                conReply.from_json(json_obj);

                LOG << "share apply connect info: " << json_obj;
                q->metaObject()->invokeMethod(CooperationManager::instance(),
                                              "handleConnectResult",
                                              Qt::QueuedConnection,
                                              Q_ARG(bool, conReply.reply));
            } break;
            case FRONT_SHARE_STOP:
                DLOG << "share stop";
                break;
            default:
                break;
            }
        }
    });

    // start ipc services
    ipc::FrontendImpl *frontendimp = new ipc::FrontendImpl();
    frontendimp->setInterface(frontendIpcSer);

    bool onlyTransfer = qApp->property("onlyTransfer").toBool();
    rpc::Server().add_service(frontendimp).start("0.0.0.0", onlyTransfer ? UNI_IPC_FRONTEND_TRANSFER_PORT : UNI_IPC_FRONTEND_COOPERATION_PORT, "/frontend", "", "");
}

QList<DeviceInfoPointer> CooperationUtilPrivate::parseDeviceInfo(const co::Json &obj)
{
    NodeList nodeList;
    nodeList.from_json(obj);

    QList<DeviceInfoPointer> devInfoList;
    for (const auto &node : nodeList.peers) {
        DeviceInfoPointer devInfo { nullptr };
        for (const auto &app : node.apps) {
            if (app.appname != CooperRegisterName)
                continue;

            QJsonParseError error;
            auto doc = QJsonDocument::fromJson(app.json.c_str(), &error);
            if (error.error != QJsonParseError::NoError)
                continue;

            auto map = doc.toVariant().toMap();
            if (!map.contains(AppSettings::DeviceNameKey))
                continue;

            map.insert("IPAddress", node.os.ipv4.c_str());
            devInfo = DeviceInfo::fromVariantMap(map);
            break;
        }

        if (devInfo && devInfo->isValid() && devInfo->discoveryMode() == DeviceInfo::DiscoveryMode::Everyone)
            devInfoList << devInfo;
    }

    return devInfoList;
}

CooperationUtil::CooperationUtil(QObject *parent)
    : QObject(parent),
      d(new CooperationUtilPrivate(this))
{
}

CooperationUtil::~CooperationUtil()
{
    d->thisDestruct = true;
}

CooperationUtil *CooperationUtil::instance()
{
    static CooperationUtil ins;
    return &ins;
}

QWidget *CooperationUtil::mainWindow()
{
    if (!d->window)
        d->window = new MainWindow;

    return d->window;
}

QString CooperationUtil::sessionId() const
{
    return d->sessionId;
}

void CooperationUtil::destroyMainWindow()
{
    if (d->window)
        delete d->window;
}

void CooperationUtil::registerDeviceOperation(const QVariantMap &map)
{
    d->window->onRegistOperations(map);
}

void CooperationUtil::registAppInfo(const QString &infoJson)
{
    LOG << "regist app info: " << infoJson.toStdString();
    if (!d->backendOk) {
        LOG << "The ping backend is false";
        return;
    }

    UNIGO([infoJson] {
        rpc::Client rpcClient("127.0.0.1", UNI_IPC_BACKEND_PORT, false);
        co::Json req, res;

        AppPeerInfo peerInfo;
        peerInfo.appname = CooperRegisterName;
        peerInfo.json = infoJson.toStdString();

        req = peerInfo.as_json();
        req.add_member("api", "Backend.registerDiscovery");
        rpcClient.call(req, res);
        rpcClient.close();
    });
}

void CooperationUtil::unregistAppInfo()
{
    if (!d->backendOk) {
        LOG << "The ping backend is false";
        return;
    }

    UNIGO([] {
        rpc::Client rpcClient("127.0.0.1", UNI_IPC_BACKEND_PORT, false);
        co::Json req, res;

        AppPeerInfo peerInfo;
        peerInfo.appname = CooperRegisterName;

        req = peerInfo.as_json();
        req.add_member("api", "Backend.unregisterDiscovery");
        rpcClient.call(req, res);
        rpcClient.close();
    });
}

void CooperationUtil::asyncDiscoveryDevice()
{
    if (!d->backendOk) {
        LOG << "The ping backend is false";
        Q_EMIT discoveryFinished({});
        return;
    }

    UNIGO([this] {
        LOG << "start discovery device";
        rpc::Client rpcClient("127.0.0.1", UNI_IPC_BACKEND_PORT, false);
        co::Json req, res;
        req.add_member("api", "Backend.getDiscovery");
        rpcClient.call(req, res);
        rpcClient.close();

        QList<DeviceInfoPointer> infoList;
        bool ok = res.get("result").as_bool();
        if (!ok) {
            WLOG << "discovery devices failed!";
        } else {
            DLOG << "all device: " << res.get("msg").as_string();
            co::Json obj;
            obj.parse_from(res.get("msg").as_string());
            infoList = d->parseDeviceInfo(obj);
        }

        Q_EMIT discoveryFinished(infoList);
    });
}

void CooperationUtil::setAppConfig(const QString &key, const QString &value)
{
    if (!d->backendOk) {
        LOG << "The ping backend is false";
        return;
    }

    UNIGO([=] {
        rpc::Client rpcClient("127.0.0.1", UNI_IPC_BACKEND_PORT, false);
        co::Json req, res;

        req = {
            { "appname", CooperRegisterName },
            { "key", key.toStdString() },
            { "value", value.toStdString() }
        };
        req.add_member("api", "Backend.setAppConfig");

        rpcClient.call(req, res);
        rpcClient.close();
    });
}

QVariantMap CooperationUtil::deviceInfo()
{
    QVariantMap info;
    auto value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::DiscoveryModeKey);
    info.insert(AppSettings::DiscoveryModeKey, value.isValid() ? value.toInt() : 0);

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::DeviceNameKey);
    info.insert(AppSettings::DeviceNameKey,
                value.isValid()
                        ? value.toString()
                        : QDir(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0)).dirName());

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::PeripheralShareKey);
    info.insert(AppSettings::PeripheralShareKey, value.isValid() ? value.toBool() : false);

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::LinkDirectionKey);
    info.insert(AppSettings::LinkDirectionKey, value.isValid() ? value.toInt() : 0);

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::TransferModeKey);
    info.insert(AppSettings::TransferModeKey, value.isValid() ? value.toInt() : 0);

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::StoragePathKey);
    auto storagePath = value.isValid() ? value.toString() : QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    info.insert(AppSettings::StoragePathKey, storagePath);

    value = ConfigManager::instance()->appAttribute(AppSettings::GenericGroup, AppSettings::ClipboardShareKey);
    info.insert(AppSettings::ClipboardShareKey, value.isValid() ? value.toBool() : false);

    return info;
}

QString CooperationUtil::localIPAddress()
{
    QString ip;
    // QNetworkInterface 类提供了一个主机 IP 地址和网络接口的列表
    foreach (QNetworkInterface netInterface, QNetworkInterface::allInterfaces()) {
        // 每个网络接口包含 0 个或多个 IP 地址
        QList<QNetworkAddressEntry> entryList = netInterface.addressEntries();
        if (netInterface.name().startsWith("virbr") || netInterface.name().startsWith("vmnet")
            || netInterface.name().startsWith("docker")) {
            // 跳过桥接，虚拟机和docker的网络接口
            DLOG << "netInterface name:" << netInterface.name().toStdString();
            continue;
        }

        // 遍历每一个 IP 地址
        foreach (QNetworkAddressEntry entry, entryList) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.ip() != QHostAddress::LocalHost) {
                //IP地址
                ip = entry.ip().toString();
                DLOG << "IP Address:" << ip.toStdString();
                return ip;
            }
        }
    }
    return ip;
}
