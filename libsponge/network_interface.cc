#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway,
//! but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // 如果目标以太网地址已经知道了，直接发送以太网数据帧
    if (_routing_table.find(next_hop_ip) != _routing_table.end()) {
        // 创建一个以太网数据帧
        EthernetFrame eframe;
        // 设置头部
        eframe.header().type = EthernetHeader::TYPE_IPv4;
        eframe.header().dst = _routing_table[next_hop_ip].first;
        eframe.header().src = _ethernet_address;
        // 设置数据部分
        eframe.payload() = dgram.serialize();
        // 发送出去
        _frames_out.push(eframe);
    } else {
        // 缓存数据报文，等待收到 ARP 回复后发送出去
        _datagram_cache.emplace_back(dgram, next_hop);

        // 发送一个 ARP 请求报文
        send_arp_message({}, next_hop_ip, ARPMessage::OPCODE_REQUEST, ETHERNET_BROADCAST);
        _arp_response[next_hop_ip] = 5 * 1000;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 如果目标地址不是发送到这里的以太网帧，或者不是广播，直接返回
    if (frame.header().dst != _ethernet_address || frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    // 收到一个 IPv4 以太网帧
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        return dgram;
    }
    // 收到一个 ARP 报文
    ARPMessage msg;
    // 如果解析 frame 是 ARP 报文出错的话，直接返回
    if (msg.parse(frame.payload()) != ParseResult::NoError) {
        return nullopt;
    }

    // 在路由表中记录IP地址以及对应的MAC地址
    uint32_t src_ip = msg.sender_ip_address;
    _routing_table[src_ip] = std::make_pair(msg.sender_ethernet_address, 30 * 1000);

    const uint32_t &dst_ip_addr = msg.target_ip_address;
    const EthernetAddress &dst_eth_addr = msg.target_ethernet_address;

    // 如果是一个发给自己的 ARP 请求
    bool is_valid_arp_request =
            msg.opcode == ARPMessage::OPCODE_REQUEST && dst_ip_addr == _ip_address.ipv4_numeric();
    bool is_valid_arp_response = msg.opcode == ARPMessage::OPCODE_REPLY && dst_eth_addr == _ethernet_address;

    // 如果收到的是发给自己的 ARP 请求报文
    if (is_valid_arp_request) {
        // 发送一个 ARP 回复报文
        send_arp_message({}, src_ip, ARPMessage::OPCODE_REPLY, msg.sender_ethernet_address);
    }
    if (!is_valid_arp_request && !is_valid_arp_response) {
        return nullopt;
    }
    // 如果是 ARP 回复报文
    for (auto it = _datagram_cache.begin(); it != _datagram_cache.end();) {
        if (it->second.ipv4_numeric() == src_ip) {
            send_datagram(it->first, it->second);
            it = _datagram_cache.erase(it);
        } else {
            ++it;
        }
    }
    _arp_response.erase(src_ip);
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 将 ARP 表中过期的条目删除
    for (auto iter = _routing_table.begin(); iter != _routing_table.end(); /* nop */) {
        if (iter->second.second <= ms_since_last_tick)
            iter = _routing_table.erase(iter);
        else {
            iter->second.second -= ms_since_last_tick;
            ++iter;
        }
    }
    // 将 ARP 等待队列中过期的条目删除
    for (auto iter = _arp_response.begin(); iter != _arp_response.end(); /* nop */) {
        // 如果 ARP 等待队列中的 ARP 请求过期
        if (iter->second <= ms_since_last_tick) {
            // 重新发送 ARP 请求
            send_arp_message({}, iter->first, ARPMessage::OPCODE_REQUEST, ETHERNET_BROADCAST);
            iter->second = 5 * 1000;
        } else {
            iter->second -= ms_since_last_tick;
            ++iter;
        }
    }
}

void NetworkInterface::send_arp_message(EthernetAddress target_address, uint32_t target_ip,
                                        uint16_t opcode, EthernetAddress eth_frame_dst) {
    ARPMessage arp_msg;
    arp_msg.opcode = opcode;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = target_address;
    arp_msg.target_ip_address = target_ip;

    EthernetFrame eth_frame;
    eth_frame.header().dst = eth_frame_dst;
    eth_frame.header().src = _ethernet_address;
    eth_frame.header().type = EthernetHeader::TYPE_ARP;
    eth_frame.payload() = arp_msg.serialize();
    _frames_out.push(eth_frame);
}
