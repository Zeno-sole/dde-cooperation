// Autogenerated.
// DO NOT EDIT. All changes will be undone.
#pragma once

#include "co/json.h"

struct NodePeerInfo {
    fastring proto_version;
    fastring uuid;
    fastring nickname;
    fastring username;
    fastring hostname;
    fastring ipv4;
    fastring share_connect_ip;
    int32 port;
    int32 os_type;
    int32 mode_type;

    void from_json(const co::Json& _x_) {
        proto_version = _x_.get("proto_version").as_c_str();
        uuid = _x_.get("uuid").as_c_str();
        nickname = _x_.get("nickname").as_c_str();
        username = _x_.get("username").as_c_str();
        hostname = _x_.get("hostname").as_c_str();
        ipv4 = _x_.get("ipv4").as_c_str();
        share_connect_ip = _x_.get("share_connect_ip").as_c_str();
        port = (int32)_x_.get("port").as_int64();
        os_type = (int32)_x_.get("os_type").as_int64();
        mode_type = (int32)_x_.get("mode_type").as_int64();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("proto_version", proto_version);
        _x_.add_member("uuid", uuid);
        _x_.add_member("nickname", nickname);
        _x_.add_member("username", username);
        _x_.add_member("hostname", hostname);
        _x_.add_member("ipv4", ipv4);
        _x_.add_member("share_connect_ip", share_connect_ip);
        _x_.add_member("port", port);
        _x_.add_member("os_type", os_type);
        _x_.add_member("mode_type", mode_type);
        return _x_;
    }
};

struct AppPeerInfo {
    fastring appname;
    fastring json;

    void from_json(const co::Json& _x_) {
        appname = _x_.get("appname").as_c_str();
        json = _x_.get("json").as_c_str();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("appname", appname);
        _x_.add_member("json", json);
        return _x_;
    }
};

struct NodeInfo {
    NodePeerInfo os;
    co::vector<AppPeerInfo> apps;

    void from_json(const co::Json& _x_) {
        os.from_json(_x_.get("os"));
        do {
            auto& _unamed_v1 = _x_.get("apps");
            for (uint32 i = 0; i < _unamed_v1.array_size(); ++i) {
                AppPeerInfo _unamed_v2;
                _unamed_v2.from_json(_unamed_v1[i]);
                apps.emplace_back(std::move(_unamed_v2));
            }
        } while (0);
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("os", os.as_json());
        do {
            co::Json _unamed_v1;
            for (size_t i = 0; i < apps.size(); ++i) {
                _unamed_v1.push_back(apps[i].as_json());
            }
            _x_.add_member("apps", _unamed_v1);
        } while (0);
        return _x_;
    }
};

struct NodeList {
    int32 code;
    co::vector<NodeInfo> peers;

    void from_json(const co::Json& _x_) {
        code = (int32)_x_.get("code").as_int64();
        do {
            auto& _unamed_v1 = _x_.get("peers");
            for (uint32 i = 0; i < _unamed_v1.array_size(); ++i) {
                NodeInfo _unamed_v2;
                _unamed_v2.from_json(_unamed_v1[i]);
                peers.emplace_back(std::move(_unamed_v2));
            }
        } while (0);
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("code", code);
        do {
            co::Json _unamed_v1;
            for (size_t i = 0; i < peers.size(); ++i) {
                _unamed_v1.push_back(peers[i].as_json());
            }
            _x_.add_member("peers", _unamed_v1);
        } while (0);
        return _x_;
    }
};

struct MiscJsonCall {
    fastring app;
    fastring json;

    void from_json(const co::Json& _x_) {
        app = _x_.get("app").as_c_str();
        json = _x_.get("json").as_c_str();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("app", app);
        _x_.add_member("json", json);
        return _x_;
    }
};

struct ApplyTransFiles {
    fastring machineName;
    fastring appname;
    fastring tarAppname;
    int32 type;
    fastring selfIp;
    int32 selfPort;

    void from_json(const co::Json& _x_) {
        machineName = _x_.get("machineName").as_c_str();
        appname = _x_.get("appname").as_c_str();
        tarAppname = _x_.get("tarAppname").as_c_str();
        type = (int32)_x_.get("type").as_int64();
        selfIp = _x_.get("selfIp").as_c_str();
        selfPort = (int32)_x_.get("selfPort").as_int64();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("machineName", machineName);
        _x_.add_member("appname", appname);
        _x_.add_member("tarAppname", tarAppname);
        _x_.add_member("type", type);
        _x_.add_member("selfIp", selfIp);
        _x_.add_member("selfPort", selfPort);
        return _x_;
    }
};

