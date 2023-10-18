// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchlight.h"
#include "co/co/sock.h"
#include "co/json.h"
#include "co/time.h"
#include "co/co.h"
#include "co/log.h"

DEF_int32(max_idle, 3000, "max_idle");
DEF_string(udp_ip, "0.0.0.0", "udp_ip");
DEF_int32(udp_port, 30001, "udp_port");
DEF_string(mcast_ip, "239.255.0.1", "mcast_ip");

namespace searchlight {

Discoverer::Discoverer(const fastring& listen_for_service,
           const on_services_changed_t on_services_changed
          )
    : _listen_for_service(listen_for_service)
    , _on_services_changed(on_services_changed)
{
    _discovered_services.clear();
}

Discoverer::~Discoverer()
{
    _stop = true;
    _discovered_services.clear();
}

void Discoverer::start()
{
    _stop = true;
    sock_t sockfd = co::udp_socket();
    if (sockfd < 0) {
        ELOG << "Failed to create socket";
        return;
    }

    struct sockaddr_in addr;
    bool init = co::init_addr(&addr, FLG_udp_ip.c_str(), FLG_udp_port);
    if (!init) {
        ELOG << "Failed to init address";
        co::close(sockfd);
        return;
    }

    int res = co::bind(sockfd, &addr, sizeof(addr));
    if (res < 0) {
        ELOG << "Failed to bind address";
        co::close(sockfd);
        return;
    }

    //给 socket 设置 SO_REUSEADDR 选项，一般 server 端的 listening socket 需要设置这个选项，防止 server 重启后 bind 失败。
    co::set_reuseaddr(sockfd);

    // 将套接字加入到组播组
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(FLG_mcast_ip.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (co::setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        ELOG << "Failed to join multicast group";
        co::close(sockfd);
        return;
    }

    struct sockaddr_in cli;
    int len = sizeof(cli);
    char buffer[1024];

    _stop = false;

    _timer.restart();
    // 定时更新发现设备
    go([this](){
        while (!_stop) {
            sleep::ms(1000); //co::sleep(1000);
            if (remove_idle_services()) {
                // callback discovery changed
                _on_services_changed(_discovered_services);
            }
        }
    });

    LOG << "discoverer server start";
    // 接收组播数据包
    while (!_stop) {
        memset(buffer, 0, sizeof(buffer));
        int recv_len = co::recvfrom(sockfd, buffer, sizeof(buffer), &cli, &len);
        if (recv_len < 0) {
            LOG << "discoverer server recvfrom error: " << co::strerror();
            break;
        }

        fastring msg(buffer, recv_len);
        handle_message(msg, co::addr2str(&cli));
    }

    // 关闭套接字
    co::close(sockfd);

    _stop = true;
}

bool Discoverer::started() {
    return !_stop;
}

void Discoverer::exit()
{
    _stop = true;
}

void Discoverer::handle_message(const fastring& message, const fastring& sender_endpoint)
{
    // 处理接收到的数据
    //LOG << "server recv " << message << " from " << sender_endpoint;
    co::Json node;
    if (!node.parse_from(message)) {
        DLOG << "Invalid service, ignore!!!!!";
        return;
    }

    fastring name = node.get("name").as_string();
    int32 port = node.get("port").as_int32();
    fastring info = node.get("info").as_string();

    fastring endpoint(sender_endpoint);
    fastring sport = sender_endpoint.substr(sender_endpoint.rfind(':') + 1);
    endpoint = endpoint.remove_suffix(sport).cat(port);

    auto discovered_service = service
        {
            name,
            endpoint,
            info,
            _timer.ms()
        };

    if (name == _listen_for_service) {
        _discovered_services.erase(discovered_service);
        _discovered_services.insert(discovered_service);

        remove_idle_services(); // update
        _on_services_changed(_discovered_services);
    } else {
        DLOG << "ignoring: " << discovered_service;
    }
}

bool Discoverer::remove_idle_services()
{
    auto dead_line = _timer.ms() - FLG_max_idle;
    bool removed = false;

    for (services::const_iterator i = _discovered_services.begin(); i != _discovered_services.end();)
    {
        if (i->last_seen < dead_line) {
            i = _discovered_services.erase(i);
            removed = true;
        } else {
            ++i;
        }
    }

    return removed;
}


Announcer::Announcer(const fastring& service_name,
          const uint16_t service_port,
          const fastring& info
          )
    : _service_name(service_name)
    , _service_port(service_port)
    , _service_info(info)
{

}

Announcer::~Announcer()
{
    _stop = true;
}

void Announcer::update(const fastring &info)
{
    _service_info = info;
}

void Announcer::start()
{
    _stop = true;
    sock_t sockfd = co::udp_socket();
    if (sockfd < 0) {
        ELOG << "Failed to create socket";
        return;
    }

    // 设置允许发送组播数据包回环：关闭，过滤来自本机的包
    int enable = 0;
    if (co::setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &enable, sizeof(enable)) < 0) {
        ELOG << "Failed to set socket options";
        co::close(sockfd);
        return;
    }

    // 填充目标组播地址和端口号
    struct sockaddr_in dest_addr;
    int len = sizeof(dest_addr);
    bool init = co::init_addr(&dest_addr, FLG_mcast_ip.c_str(), FLG_udp_port);
    if (!init) {
        ELOG << "Failed to set destination address";
        co::close(sockfd);
        return;
    }

    _stop = false;

    co::Json node;
    node.add_member("name", _service_name);
    node.add_member("port", _service_port);

    LOG << "announcer server start";
    // 发送数据包 int sendto(sock_t fd, const void* buf, int n, const void* dst_addr, int addrlen, int ms=-1);
    while (!_stop) {
        // update the last info
        node.remove("info");
        node.add_member("info", _service_info);
        fastring message = node.str();

        //DLOG << "UDP send: " << message;

        int send_len = co::sendto(sockfd, message.data(), message.size(), &dest_addr, len);
        if (send_len < 0) {
            ELOG << "Failed to send data";
            break;
        }

        co::sleep(1000); // announcer every second
    }

    // 关闭套接字
    co::close(sockfd);

    _stop = true;
}

bool Announcer::started() {
    return !_stop;
}

void Announcer::exit()
{
    _stop = true;
}

} // searchlight