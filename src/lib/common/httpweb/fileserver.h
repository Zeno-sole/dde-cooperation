// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILESERVER_H
#define FILESERVER_H

#include "webbinder.h"

//#include "server/http/https_server.h"
#include "server/http/http_server.h"
#include "string/string_utils.h"
#include "syncstatus.h"

#include <iostream>
#include <map>
#include <mutex>

class FileServer : public WebInterface, public CppServer::HTTP::HTTPServer
{
    using CppServer::HTTP::HTTPServer::HTTPServer;

public:

    bool start();
    bool stop();

    int webBind(std::string webDir, std::string diskDir);
    int webUnbind(std::string webDir);
    void clearBind();

    std::string genToken(std::string info);
    bool verifyToken(std::string &token);

protected:
    std::shared_ptr<CppServer::Asio::TCPSession> CreateSession(const std::shared_ptr<CppServer::Asio::TCPServer> &server) override;
    void onError(int error, const std::string &category, const std::string &message) override;

private:

};

#endif // FILESERVER_H
