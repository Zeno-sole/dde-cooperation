﻿// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "handleipcservice.h"
#include "sendipcservice.h"
#include "service/rpc/sendrpcservice.h"
#include "ipc/proto/chan.h"
#include "ipc/proto/comstruct.h"
#include "ipc/backendservice.h"
#include "common/constant.h"
#include "service/comshare.h"
#include "service/discoveryjob.h"
#include "utils/config.h"
#include "service/jobmanager.h"

#include <QPointer>

HandleIpcService::HandleIpcService(QObject *parent)
    : QObject(parent)
{
    // init and start backend IPC
    ipcServiceStart();
}

HandleIpcService::~HandleIpcService()
{
}

void HandleIpcService::ipcServiceStart()
{
    createIpcBackend(UNI_IPC_BACKEND_PORT);
    createIpcBackend(UNI_IPC_BACKEND_COOPER_TRAN_PORT);
    createIpcBackend(UNI_IPC_BACKEND_DATA_TRAN_PORT);
}

void HandleIpcService::createIpcBackend(const quint16 port)
{
    if (_backendIpcServices.contains(port)) {
        ELOG << "this port has backend!!!!!! port = " << port;
        return;
    }
    QSharedPointer<BackendService> _backendIpcService(new BackendService);
    _backendIpcServices.insert(port, _backendIpcService);

    QPointer<HandleIpcService> self = this;
    UNIGO([self, _backendIpcService]() {
        while(!self.isNull()) {
            BridgeJsonData bridge;
            _backendIpcService->bridgeChan()->operator>>(bridge); //300ms超时
            if (!_backendIpcService->bridgeChan()->done()) {
                // timeout, next read
                continue;
            }

            LOG << "HandleIpcService get bridge json: " << bridge.type << " json:" << bridge.json;
            co::Json json_obj = json::parse(bridge.json);
            if (json_obj.is_null()) {
                ELOG << "parse error from: " << bridge.json;
                continue;
            }
            self->handleAllMsg(_backendIpcService, bridge.type, json_obj);
        }
    });

    connect(this, &HandleIpcService::connectClosed, this, &HandleIpcService::handleConnectClosed);
    // start ipc services
    ipc::BackendImpl *backendimp = new ipc::BackendImpl();
    backendimp->setInterface(_backendIpcService.data());
    rpc::Server().add_service(backendimp, [this](int type, const fastring &ip, const uint16 port){
        Q_UNUSED(ip);
        if (type == 0)
            emit this->connectClosed(port);
    }).start("0.0.0.0", port, "/backend",
             QString::number(quintptr(_backendIpcService.data())).toStdString().c_str(), "");
}

void HandleIpcService::handleAllMsg(const QSharedPointer<BackendService> backend, const uint type, co::Json &msg)
{
    switch (type) {
    case PING:
    {
        BridgeJsonData res;
        res.type = PING;
        res.json = handlePing(msg).toStdString();

        backend->bridgeResult()->operator<<(res);
        break;
    }
    case MISC_MSG:
    {
        MiscJsonCall call;
        call.from_json(msg);
        SendRpcService::instance()->doMisc(call.app.c_str(), call.json.c_str());
        break;
    }
    case BACK_TRY_CONNECT:
    {
        handleTryConnect(msg);
        break;
    }
    case BACK_TRY_TRANS_FILES:
    {
        ipc::TransFilesParam param;
        param.from_json(msg);
        QString session = QString(param.session.c_str());
        QString savedir = QString(param.savedir.c_str());
        QStringList paths;
        for (uint32 i = 0; i < param.paths.size(); i++) {
            paths << param.paths[i].c_str();
        }

        newTransSendJob(session, param.targetSession.c_str(), param.id, paths, param.sub, savedir);
        break;
    }
    case BACK_RESUME_JOB:
    case BACK_CANCEL_JOB:
    {
        bool ok = JobManager::instance()->doJobAction(type, msg);
        co::Json resjson = {
            { "result", ok },
            { "msg", msg.str() }
        };

        BridgeJsonData res;
        res.type = type;
        res.json = resjson.str();

        backend->bridgeResult()->operator<<(res);
        break;
    }
    case BACK_GET_DISCOVERY:
    {
        handleGetAllNodes(backend);
        break;
    }
    case BACK_GET_PEER:
    {
        break;
    }
    case BACK_FS_CREATE:
    {
        break;
    }
    case BACK_FS_DELETE:
    {
        break;
    }
    case BACK_FS_RENAME:
    {
        break;
    }
    case BACK_FS_PULL:
    {
        break;
    }
    case BACK_DISC_REGISTER:
    {
        handleNodeRegister(false, msg);
        break;
    }
    case BACK_DISC_UNREGISTER:
    {
        handleNodeRegister(true, msg);
        break;
    }
    case BACK_APPLY_TRANS_FILES:
    {
        handleBackApplyTransFiles(msg);
        break;
    }
    default:
        break;
    }
}

QString HandleIpcService::handlePing(const co::Json &msg)
{
    //check session or gen new one
    ipc::PingBackParam param;
    param.from_json(msg);

    fastring my_ver(BACKEND_PROTO_VERSION);
    if (my_ver.compare(param.version) != 0) {
        DLOG << param.version << " =version not match= " << my_ver;
        return QString();
    }
    QString appName = param.who.c_str();
    QString sesid = _sessionIDs.value(param.who.c_str());
    if (!sesid.isEmpty())
        return sesid;

    // gen new one
    sesid = co::randstr(appName.toStdString().c_str(), 8).c_str(); // 长度为8的16进制字符串
    _sessionIDs.insert(appName, sesid);
    // 创建新的sessionid
    SendIpcService::instance()->handleSaveSession(appName, sesid, static_cast<uint16>(param.cb_port));
    return sesid;
}

void HandleIpcService::newTransSendJob(QString session, const QString targetSession, int32 jobId, QStringList paths, bool sub, QString savedir)
{
    auto s = _sessionIDs.key(session);
    if (s.isEmpty()) {
        DLOG << "this session is invalid." << session.toStdString();
        return;
    }

    int32 id = jobId;
    fastring who = s.toStdString();
    fastring savepath = savedir.toStdString();

    co::Json pathjson;
    for (QString path : paths) {
        fastring filepath = path.toStdString();
        pathjson.push_back(filepath);
    }
    //FSJob
    co::Json jobjson = {
        { "who", who },
        { "targetwho", targetSession.toStdString() },
        { "job_id", id },
        { "path", pathjson.str() },
        { "save", savepath },
        { "hidden", false },
        { "sub", sub },
        { "write", false }
    };

    SendIpcService::instance()->handleAddJob(s, jobId);
    JobManager::instance()->handleRemoteRequestJob(jobjson.str().c_str());
}

void HandleIpcService::handleNodeRegister(bool unreg, const co::Json &info)
{
    AppPeerInfo appPeer;
    appPeer.from_json(info);
    if (unreg) {
        fastring appname = appPeer.appname;
        // 移除ping
        SendRpcService::instance()->removePing(appname.c_str());
        SendIpcService::instance()->removeSessionByAppName(appname.c_str());
    }
    DiscoveryJob::instance()->updateAnnouncApp(unreg, info.as_string());
}

void HandleIpcService::handleGetAllNodes(const QSharedPointer<BackendService> _backendIpcService)
{
    auto nodes = DiscoveryJob::instance()->getNodes();
    NodeList nodeInfos;
    for (const auto &node : nodes) {
        co::Json nodejs;
        nodejs.parse_from(node);
        NodeInfo info;
        info.from_json(nodejs);
        nodeInfos.peers.push_back(info);
    }
    BridgeJsonData res;
    res.type = BACK_GET_DISCOVERY;
    res.json = nodeInfos.as_json().str();

    _backendIpcService->bridgeResult()->operator<<(res);
}

void HandleIpcService::handleBackApplyTransFiles(co::Json param)
{
     // 远程发送
    SendRpcService::instance()->doSendApplyTransFiles(param);
    // todo, servicemanager收到信号处理
}

void HandleIpcService::handleConnectClosed(const quint16 port)
{
    // 不延时，还是可以ping通，资源还没有回收
    QTimer::singleShot(1000, this, [port]{
        SendIpcService::instance()->handleConnectClosed(port);
    });
}

void HandleIpcService::handleTryConnect(co::Json json)
{
    UNIGO([json]() {
        ipc::ConnectParam param;
        param.from_json(json);
        QString session(param.session.c_str());
        QString ip(param.host.c_str());
        QString pass(param.password.c_str());
        LOG << " rcv client connet  " << ip.toStdString() << session.toStdString();
        SendRpcService::instance()->doLogin(session, ip, UNI_RPC_PORT_BASE, session, param.password.c_str());
        // todo, servicemanager收到信号处理
//        LoginResultStruct ru;
//        ru.from_json(result);
//        co::Json req;
//        //cbConnect {GenericResult}
//        req = {
//            { "id", 0 },
//            { "result", ru.result ? 1 : 0 },
//            { "msg", ru.appName },
//        };
//        req.add_member("api", "Frontend.cbConnect");
//        SendIpcService::instance()->handleSendToClient(ru.appName.c_str(), req.str().c_str());
    });
}