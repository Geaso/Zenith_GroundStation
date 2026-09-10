#pragma once
// Shared by Zenith ROS1 and GroundStationQt. Wire contract: docs/MAVLINK2_PROTOCOL.md.
// Instances own MAVLink parser/sequence state; callers serialize access per link.
#include <mavlink/v2.0/common/mavlink.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace zenith { namespace mavlink {
using Bytes = std::vector<uint8_t>;
constexpr uint16_t kExtensionType = 32768;
constexpr uint8_t kComponent = 191;
constexpr size_t kHeaderSize = 19;
constexpr size_t kFragmentSize = 249 - kHeaderSize;
constexpr size_t kMaxPayload = 256 * 1024;
constexpr size_t kMaxAssemblies = 16;
constexpr uint64_t kTimeoutMs = 5000;
inline uint32_t get32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
inline uint16_t get16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
inline void put32(uint8_t* p, uint32_t n) { for (unsigned i=0;i<4;++i) p[i]=uint8_t(n>>(i*8)); }
inline void put16(uint8_t* p, uint16_t n) { p[0]=uint8_t(n); p[1]=uint8_t(n>>8); }
inline Bytes frame(const mavlink_message_t& message) {
    Bytes bytes(MAVLINK_MAX_PACKET_LEN);
    bytes.resize(mavlink_msg_to_send_buffer(bytes.data(), &message));
    return bytes;
}
class Parser {
    mavlink_message_t buffer_{};
    mavlink_status_t status_{};
public:
    std::optional<mavlink_message_t> consume(uint8_t byte) {
        mavlink_message_t message{}; mavlink_status_t status{};
        auto result=mavlink_frame_char_buffer(&buffer_, &status_, byte, &message, &status);
        if (result == MAVLINK_FRAMING_OK) return message;
        if (result == MAVLINK_FRAMING_BAD_CRC || result == MAVLINK_FRAMING_BAD_SIGNATURE) {
            // Mirror official mavlink_parse_char resynchronization without global channel state.
            status_.parse_state = MAVLINK_PARSE_STATE_IDLE;
            if (byte == MAVLINK_STX) { status_.parse_state=MAVLINK_PARSE_STATE_GOT_STX; buffer_.magic=byte; status_.packet_idx=0; mavlink_start_checksum(&buffer_); }
        }
        return std::nullopt;
    }
    void reset() { buffer_={}; status_={}; }
};
class Encoder {
    uint8_t system_, component_;
    mavlink_status_t status_{};
public:
    explicit Encoder(uint8_t system=1, uint8_t component=kComponent):system_(system),component_(component) {}
    void setSystem(uint8_t system) { if (system) system_=system; }
    Bytes encode(mavlink_message_t message) {
        const auto* entry=mavlink_get_msg_entry(message.msgid);
        if (!entry) throw std::invalid_argument("Unknown MAVLink message id");
        message.incompat_flags=0; message.compat_flags=0;
        mavlink_finalize_message_buffer(&message,system_,component_,&status_,entry->min_msg_len,entry->max_msg_len,entry->crc_extra);
        return frame(message);
    }
    std::vector<Bytes> encodeExtension(uint8_t kind, uint32_t transaction, const Bytes& payload,
                                      uint8_t encoding=1, uint8_t targetSystem=0, uint8_t targetComponent=0) {
        if (payload.empty() || payload.size()>kMaxPayload || encoding!=1) return {};
        std::vector<Bytes> result;
        result.reserve((payload.size()+kFragmentSize-1)/kFragmentSize);
        for (size_t offset=0;offset<payload.size();offset+=kFragmentSize) {
            mavlink_v2_extension_t extension{};
            extension.message_type=kExtensionType;
            extension.target_system=targetSystem; extension.target_component=targetComponent;
            auto* p=extension.payload;
            p[0]='Z'; p[1]='N'; p[2]=1; p[3]=kind;
            put32(p+4,transaction); put32(p+8,uint32_t(payload.size())); put32(p+12,uint32_t(offset));
            const size_t n=std::min(kFragmentSize,payload.size()-offset);
            put16(p+16,uint16_t(n)); p[18]=encoding;
            std::copy_n(payload.data()+offset,n,p+kHeaderSize);
            mavlink_message_t message{};
            message.msgid=MAVLINK_MSG_ID_V2_EXTENSION;
            // MAVLink field layout is explicitly encoded; no native struct padding/endianness dependence.
            auto* wire=reinterpret_cast<uint8_t*>(_MAV_PAYLOAD_NON_CONST(&message));
            put16(wire,kExtensionType); wire[2]=0; wire[3]=targetSystem; wire[4]=targetComponent;
            std::copy_n(p,249,wire+5);
            result.push_back(encode(message));
        }
        return result;
    }
};
struct Extension {
    uint8_t kind=0, encoding=0, source_system=0, source_component=0, target_system=0, target_component=0;
    uint32_t transaction_id=0;
    Bytes payload;
};
class Reassembler {
    using Key=std::tuple<uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint32_t>;
    struct Assembly { Extension output; uint64_t started=0; std::vector<bool> received; size_t count=0; };
    std::map<Key,Assembly> active_;
public:
    void reset() { active_.clear(); }
    size_t pending() const { return active_.size(); }
    void expire(uint64_t now) {
        for(auto it=active_.begin();it!=active_.end();) {
            if(now < it->second.started || now-it->second.started>=kTimeoutMs) it=active_.erase(it); else ++it;
        }
    }
    std::optional<Extension> accept(const mavlink_message_t& message,uint64_t now) {
        expire(now);
        if(message.magic!=MAVLINK_STX || message.msgid!=MAVLINK_MSG_ID_V2_EXTENSION) return std::nullopt;
        mavlink_v2_extension_t ext{}; mavlink_msg_v2_extension_decode(&message,&ext);
        const auto* p=ext.payload;
        if(ext.message_type!=kExtensionType || ext.target_network!=0 || p[0]!='Z' || p[1]!='N' || p[2]!=1 || p[18]!=1) return std::nullopt;
        const uint32_t tx=get32(p+4),total=get32(p+8),offset=get32(p+12);
        const uint16_t n=get16(p+16);
        const Key key{message.sysid,message.compid,ext.target_system,ext.target_component,p[3],tx};
        // Canonical aligned fragments disallow overlap, holes, and inconsistent final lengths.
        if(!total || total>kMaxPayload || offset>=total || offset%kFragmentSize || !n || n>kFragmentSize || n!=std::min<size_t>(kFragmentSize,total-offset)) {
            active_.erase(key); return std::nullopt;
        }
        auto it=active_.find(key);
        if(it==active_.end()) {
            if(active_.size()>=kMaxAssemblies) return std::nullopt;
            Assembly a; a.started=now;
            a.output.kind=p[3]; a.output.encoding=p[18]; a.output.source_system=message.sysid; a.output.source_component=message.compid;
            a.output.target_system=ext.target_system; a.output.target_component=ext.target_component; a.output.transaction_id=tx;
            a.output.payload.resize(total); a.received.resize((total+kFragmentSize-1)/kFragmentSize);
            it=active_.emplace(key,std::move(a)).first;
        }
        auto& a=it->second;
        if(a.output.payload.size()!=total || a.output.encoding!=p[18]) { active_.erase(it); return std::nullopt; }
        const size_t index=offset/kFragmentSize;
        if(a.received[index]) {
            if(!std::equal(p+kHeaderSize,p+kHeaderSize+n,a.output.payload.begin()+offset)) active_.erase(it);
            return std::nullopt;
        }
        std::copy_n(p+kHeaderSize,n,a.output.payload.begin()+offset); a.received[index]=true; ++a.count;
        if(a.count==a.received.size()) { auto result=std::move(a.output); active_.erase(it); return result; }
        return std::nullopt;
    }
};
}} // namespace zenith::mavlink