﻿// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

//#include <unistd.h>
#include <string.h>
#include "tcpbuffer.h"
#include "co/log.h"

namespace zrpc_ns {

TcpBuffer::TcpBuffer(int size) {
    m_buffer.resize(size);
}

TcpBuffer::~TcpBuffer() {
}

int TcpBuffer::readAble() {

    return m_write_index - m_read_index;
}

int TcpBuffer::writeAble() {

    return m_buffer.size() - m_write_index;
}

int TcpBuffer::readIndex() const {
    return m_read_index;
}

int TcpBuffer::writeIndex() const {
    return m_write_index;
}

void TcpBuffer::resizeBuffer(int size) {
    std::vector<char> tmp(size);
    int c = std::min(size, readAble());
    if(m_buffer.size() > m_read_index)
        memcpy(&tmp[0], &m_buffer[m_read_index], c);

    m_buffer.swap(tmp);
    m_read_index = 0;
    m_write_index = m_read_index + c;
}

void TcpBuffer::writeToBuffer(const char *buf, int size) {
    if (size > writeAble()) {
        int new_size = (int)(1.5 * (m_write_index + size));
        resizeBuffer(new_size);
    }
    memcpy(&m_buffer[m_write_index], buf, size);
    m_write_index += size;
}

void TcpBuffer::readFromBuffer(std::vector<char> &re, int size) {
    if (readAble() == 0) {
        DLOG << "read buffer empty!";
        return;
    }
    int read_size = readAble() > size ? size : readAble();
    std::vector<char> tmp(read_size);

    // std::copy(m_read_index, m_read_index + read_size, tmp);
    memcpy(&tmp[0], &m_buffer[m_read_index], read_size);
    re.swap(tmp);
    m_read_index += read_size;
    adjustBuffer();
}

void TcpBuffer::adjustBuffer() {
    if (m_read_index > static_cast<int>(m_buffer.size() / 3)) {

        std::vector<char> new_buffer(m_buffer.size());

        int count = readAble();
        // std::copy(&m_buffer[m_read_index], readAble(), &new_buffer);
        memcpy(&new_buffer[0], &m_buffer[m_read_index], count);

        m_buffer.swap(new_buffer);
        m_write_index = count;
        m_read_index = 0;
        new_buffer.clear();
    }
}

int TcpBuffer::getSize() {
    return m_buffer.size();
}

void TcpBuffer::clearBuffer() {
    m_buffer.clear();
    m_read_index = 0;
    m_write_index = 0;
}

void TcpBuffer::recycleRead(int index) {
    int j = m_read_index + index;
    if (j > (int)m_buffer.size()) {
        ELOG << "recycleRead error";
        return;
    }
    m_read_index = j;
    adjustBuffer();
}

void TcpBuffer::recycleWrite(int index) {
    int j = m_write_index + index;
    if (j > (int)m_buffer.size()) {
        ELOG << "recycleWrite error, j=" << j;
        return;
    }
    m_write_index = j;
    adjustBuffer();
}

std::string TcpBuffer::getBufferString() {
    std::string re(static_cast<size_t>(readAble()), '0');
    memcpy(&re[0], &m_buffer[static_cast<size_t>(m_read_index)], static_cast<size_t>(readAble()));
    return re;
}

std::vector<char> TcpBuffer::getBufferVector() {
    return m_buffer;
}

} // namespace zrpc_ns
