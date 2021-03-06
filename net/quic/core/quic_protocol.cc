// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_protocol.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/core/quic_flags.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/core/quic_versions.h"

using base::StringPiece;
using std::map;
using std::numeric_limits;
using std::ostream;
using std::string;

namespace net {

size_t GetPacketHeaderSize(QuicVersion version,
                           const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header.public_header.connection_id_length,
                             header.public_header.version_flag,
                             header.public_header.multipath_flag,
                             header.public_header.nonce != nullptr,
                             header.public_header.packet_number_length);
}

size_t GetPacketHeaderSize(QuicVersion version,
                           QuicConnectionIdLength connection_id_length,
                           bool include_version,
                           bool include_path_id,
                           bool include_diversification_nonce,
                           QuicPacketNumberLength packet_number_length) {
  return kPublicFlagsSize + connection_id_length +
         (include_version ? kQuicVersionSize : 0) +
         (include_path_id ? kQuicPathIdSize : 0) + packet_number_length +
         (include_diversification_nonce ? kDiversificationNonceSize : 0);
}

size_t GetStartOfEncryptedData(QuicVersion version,
                               const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header);
}

size_t GetStartOfEncryptedData(QuicVersion version,
                               QuicConnectionIdLength connection_id_length,
                               bool include_version,
                               bool include_path_id,
                               bool include_diversification_nonce,
                               QuicPacketNumberLength packet_number_length) {
  // Encryption starts before private flags.
  return GetPacketHeaderSize(version, connection_id_length, include_version,
                             include_path_id, include_diversification_nonce,
                             packet_number_length);
}

QuicPacketPublicHeader::QuicPacketPublicHeader()
    : connection_id(0),
      connection_id_length(PACKET_8BYTE_CONNECTION_ID),
      multipath_flag(false),
      reset_flag(false),
      version_flag(false),
      packet_number_length(PACKET_6BYTE_PACKET_NUMBER),
      nonce(nullptr) {}

QuicPacketPublicHeader::QuicPacketPublicHeader(
    const QuicPacketPublicHeader& other) = default;

QuicPacketPublicHeader::~QuicPacketPublicHeader() {}

QuicPacketHeader::QuicPacketHeader()
    : packet_number(0), path_id(kDefaultPathId) {}

QuicPacketHeader::QuicPacketHeader(const QuicPacketPublicHeader& header)
    : public_header(header), packet_number(0), path_id(kDefaultPathId) {}

QuicPacketHeader::QuicPacketHeader(const QuicPacketHeader& other) = default;

QuicPublicResetPacket::QuicPublicResetPacket()
    : nonce_proof(0), rejected_packet_number(0) {}

QuicPublicResetPacket::QuicPublicResetPacket(
    const QuicPacketPublicHeader& header)
    : public_header(header), nonce_proof(0), rejected_packet_number(0) {}

void StreamBufferDeleter::operator()(char* buffer) const {
  if (allocator_ != nullptr && buffer != nullptr) {
    allocator_->Delete(buffer);
  }
}

UniqueStreamBuffer NewStreamBuffer(QuicBufferAllocator* allocator,
                                   size_t size) {
  return UniqueStreamBuffer(allocator->New(size),
                            StreamBufferDeleter(allocator));
}

QuicStreamFrame::QuicStreamFrame()
    : QuicStreamFrame(0, false, 0, nullptr, 0, nullptr) {}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 QuicStreamOffset offset,
                                 StringPiece data)
    : QuicStreamFrame(stream_id,
                      fin,
                      offset,
                      data.data(),
                      data.length(),
                      nullptr) {}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 QuicStreamOffset offset,
                                 QuicPacketLength data_length,
                                 UniqueStreamBuffer buffer)
    : QuicStreamFrame(stream_id,
                      fin,
                      offset,
                      nullptr,
                      data_length,
                      std::move(buffer)) {
  DCHECK(this->buffer != nullptr);
  DCHECK_EQ(data_buffer, this->buffer.get());
}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 QuicStreamOffset offset,
                                 const char* data_buffer,
                                 QuicPacketLength data_length,
                                 UniqueStreamBuffer buffer)
    : stream_id(stream_id),
      fin(fin),
      data_length(data_length),
      data_buffer(data_buffer),
      offset(offset),
      buffer(std::move(buffer)) {
  if (this->buffer != nullptr) {
    DCHECK(data_buffer == nullptr);
    this->data_buffer = this->buffer.get();
  }
}

QuicStreamFrame::~QuicStreamFrame() {}

ostream& operator<<(ostream& os, const QuicPacketHeader& header) {
  os << "{ connection_id: " << header.public_header.connection_id
     << ", connection_id_length: " << header.public_header.connection_id_length
     << ", packet_number_length: " << header.public_header.packet_number_length
     << ", multipath_flag: " << header.public_header.multipath_flag
     << ", reset_flag: " << header.public_header.reset_flag
     << ", version_flag: " << header.public_header.version_flag;
  if (header.public_header.version_flag) {
    os << ", version:";
    for (size_t i = 0; i < header.public_header.versions.size(); ++i) {
      os << " ";
      os << QuicVersionToString(header.public_header.versions[i]);
    }
  }
  if (header.public_header.nonce != nullptr) {
    os << ", diversification_nonce: "
       << QuicUtils::HexEncode(StringPiece(header.public_header.nonce->data(),
                                           header.public_header.nonce->size()));
  }
  os << ", path_id: " << static_cast<int>(header.path_id)
     << ", packet_number: " << header.packet_number << " }\n";
  return os;
}

bool IsAwaitingPacket(const QuicAckFrame& ack_frame,
                      QuicPacketNumber packet_number,
                      QuicPacketNumber peer_least_packet_awaiting_ack) {
  return packet_number >= peer_least_packet_awaiting_ack &&
         !ack_frame.packets.Contains(packet_number);
}

QuicStopWaitingFrame::QuicStopWaitingFrame()
    : path_id(kDefaultPathId), least_unacked(0) {}

QuicStopWaitingFrame::~QuicStopWaitingFrame() {}

QuicAckFrame::QuicAckFrame()
    : largest_observed(0),
      ack_delay_time(QuicTime::Delta::Infinite()),
      path_id(kDefaultPathId) {}

QuicAckFrame::QuicAckFrame(const QuicAckFrame& other) = default;

QuicAckFrame::~QuicAckFrame() {}

QuicRstStreamFrame::QuicRstStreamFrame()
    : stream_id(0), error_code(QUIC_STREAM_NO_ERROR), byte_offset(0) {}

QuicRstStreamFrame::QuicRstStreamFrame(QuicStreamId stream_id,
                                       QuicRstStreamErrorCode error_code,
                                       QuicStreamOffset bytes_written)
    : stream_id(stream_id),
      error_code(error_code),
      byte_offset(bytes_written) {}

QuicConnectionCloseFrame::QuicConnectionCloseFrame()
    : error_code(QUIC_NO_ERROR) {}

QuicFrame::QuicFrame() {}

QuicFrame::QuicFrame(QuicPaddingFrame padding_frame)
    : type(PADDING_FRAME), padding_frame(padding_frame) {}

QuicFrame::QuicFrame(QuicStreamFrame* stream_frame)
    : type(STREAM_FRAME), stream_frame(stream_frame) {}

QuicFrame::QuicFrame(QuicAckFrame* frame) : type(ACK_FRAME), ack_frame(frame) {}

QuicFrame::QuicFrame(QuicMtuDiscoveryFrame frame)
    : type(MTU_DISCOVERY_FRAME), mtu_discovery_frame(frame) {}

QuicFrame::QuicFrame(QuicStopWaitingFrame* frame)
    : type(STOP_WAITING_FRAME), stop_waiting_frame(frame) {}

QuicFrame::QuicFrame(QuicPingFrame frame)
    : type(PING_FRAME), ping_frame(frame) {}

QuicFrame::QuicFrame(QuicRstStreamFrame* frame)
    : type(RST_STREAM_FRAME), rst_stream_frame(frame) {}

QuicFrame::QuicFrame(QuicConnectionCloseFrame* frame)
    : type(CONNECTION_CLOSE_FRAME), connection_close_frame(frame) {}

QuicFrame::QuicFrame(QuicGoAwayFrame* frame)
    : type(GOAWAY_FRAME), goaway_frame(frame) {}

QuicFrame::QuicFrame(QuicWindowUpdateFrame* frame)
    : type(WINDOW_UPDATE_FRAME), window_update_frame(frame) {}

QuicFrame::QuicFrame(QuicBlockedFrame* frame)
    : type(BLOCKED_FRAME), blocked_frame(frame) {}

QuicFrame::QuicFrame(QuicPathCloseFrame* frame)
    : type(PATH_CLOSE_FRAME), path_close_frame(frame) {}

void DeleteFrames(QuicFrames* frames) {
  for (QuicFrame& frame : *frames) {
    switch (frame.type) {
      // Frames smaller than a pointer are inlined, so don't need to be deleted.
      case PADDING_FRAME:
      case MTU_DISCOVERY_FRAME:
      case PING_FRAME:
        break;
      case STREAM_FRAME:
        delete frame.stream_frame;
        break;
      case ACK_FRAME:
        delete frame.ack_frame;
        break;
      case STOP_WAITING_FRAME:
        delete frame.stop_waiting_frame;
        break;
      case RST_STREAM_FRAME:
        delete frame.rst_stream_frame;
        break;
      case CONNECTION_CLOSE_FRAME:
        delete frame.connection_close_frame;
        break;
      case GOAWAY_FRAME:
        delete frame.goaway_frame;
        break;
      case BLOCKED_FRAME:
        delete frame.blocked_frame;
        break;
      case WINDOW_UPDATE_FRAME:
        delete frame.window_update_frame;
        break;
      case PATH_CLOSE_FRAME:
        delete frame.path_close_frame;
        break;
      case NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot delete type: " << frame.type;
    }
  }
  frames->clear();
}

void RemoveFramesForStream(QuicFrames* frames, QuicStreamId stream_id) {
  QuicFrames::iterator it = frames->begin();
  while (it != frames->end()) {
    if (it->type != STREAM_FRAME || it->stream_frame->stream_id != stream_id) {
      ++it;
      continue;
    }
    delete it->stream_frame;
    it = frames->erase(it);
  }
}

ostream& operator<<(ostream& os, const QuicStopWaitingFrame& sent_info) {
  os << "{ least_unacked: " << sent_info.least_unacked << " }\n";
  return os;
}

PacketNumberQueue::PacketNumberQueue() = default;
PacketNumberQueue::PacketNumberQueue(const PacketNumberQueue& other) = default;
// TODO(rtenneti): on windows RValue reference gives errors.
// PacketNumberQueue::PacketNumberQueue(PacketNumberQueue&& other) = default;
PacketNumberQueue::~PacketNumberQueue() {}

PacketNumberQueue& PacketNumberQueue::operator=(
    const PacketNumberQueue& other) = default;
// TODO(rtenneti): on windows RValue reference gives errors.
// PacketNumberQueue& PacketNumberQueue::operator=(PacketNumberQueue&& other) =
//    default;

void PacketNumberQueue::Add(QuicPacketNumber packet_number) {
  packet_number_intervals_.Add(packet_number, packet_number + 1);
}

void PacketNumberQueue::Add(QuicPacketNumber lower, QuicPacketNumber higher) {
  packet_number_intervals_.Add(lower, higher);
}

void PacketNumberQueue::Remove(QuicPacketNumber packet_number) {
  packet_number_intervals_.Difference(packet_number, packet_number + 1);
}

void PacketNumberQueue::Remove(QuicPacketNumber lower,
                               QuicPacketNumber higher) {
  packet_number_intervals_.Difference(lower, higher);
}

bool PacketNumberQueue::RemoveUpTo(QuicPacketNumber higher) {
  if (Empty()) {
    return false;
  }
  const QuicPacketNumber old_min = Min();
  packet_number_intervals_.Difference(0, higher);
  return Empty() || old_min != Min();
}

void PacketNumberQueue::Complement() {
  if (Empty()) {
    return;
  }
  packet_number_intervals_.Complement(Min(), Max() + 1);
}

bool PacketNumberQueue::Contains(QuicPacketNumber packet_number) const {
  return packet_number_intervals_.Contains(packet_number);
}

bool PacketNumberQueue::Empty() const {
  return packet_number_intervals_.Empty();
}

QuicPacketNumber PacketNumberQueue::Min() const {
  DCHECK(!Empty());
  return packet_number_intervals_.begin()->min();
}

QuicPacketNumber PacketNumberQueue::Max() const {
  DCHECK(!Empty());
  return packet_number_intervals_.rbegin()->max() - 1;
}

size_t PacketNumberQueue::NumPacketsSlow() const {
  size_t num_packets = 0;
  for (const auto& interval : packet_number_intervals_) {
    num_packets += interval.Length();
  }
  return num_packets;
}

size_t PacketNumberQueue::NumIntervals() const {
  return packet_number_intervals_.Size();
}

QuicPacketNumber PacketNumberQueue::LastIntervalLength() const {
  DCHECK(!Empty());
  return packet_number_intervals_.rbegin()->Length();
}

PacketNumberQueue::const_iterator PacketNumberQueue::lower_bound(
    QuicPacketNumber packet_number) const {
  // lower_bound returns the first interval that contains |packet_number| or the
  // first interval after |packet_number|.
  auto itr = packet_number_intervals_.Find(packet_number);
  if (itr != packet_number_intervals_.end()) {
    return itr;
  }
  for (itr = packet_number_intervals_.begin();
       itr != packet_number_intervals_.end(); ++itr) {
    if (packet_number < itr->min()) {
      return itr;
    }
  }
  return packet_number_intervals_.end();
}

PacketNumberQueue::const_iterator PacketNumberQueue::begin() const {
  return packet_number_intervals_.begin();
}

PacketNumberQueue::const_iterator PacketNumberQueue::end() const {
  return packet_number_intervals_.end();
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rbegin() const {
  return packet_number_intervals_.rbegin();
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rend() const {
  return packet_number_intervals_.rend();
}

ostream& operator<<(ostream& os, const PacketNumberQueue& q) {
  for (const Interval<QuicPacketNumber>& interval : q) {
    for (QuicPacketNumber packet_number = interval.min();
         packet_number < interval.max(); ++packet_number) {
      os << packet_number << " ";
    }
  }
  return os;
}

ostream& operator<<(ostream& os, const QuicAckFrame& ack_frame) {
  os << "{ largest_observed: " << ack_frame.largest_observed
     << ", ack_delay_time: " << ack_frame.ack_delay_time.ToMicroseconds()
     << ", packets: [ " << ack_frame.packets << " ]"
     << ", received_packets: [ ";
  for (const std::pair<QuicPacketNumber, QuicTime>& p :
       ack_frame.received_packet_times) {
    os << p.first << " at " << p.second.ToDebuggingValue() << " ";
  }
  os << " ] }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicFrame& frame) {
  switch (frame.type) {
    case PADDING_FRAME: {
      os << "type { PADDING_FRAME } " << frame.padding_frame;
      break;
    }
    case RST_STREAM_FRAME: {
      os << "type { RST_STREAM_FRAME } " << *(frame.rst_stream_frame);
      break;
    }
    case CONNECTION_CLOSE_FRAME: {
      os << "type { CONNECTION_CLOSE_FRAME } "
         << *(frame.connection_close_frame);
      break;
    }
    case GOAWAY_FRAME: {
      os << "type { GOAWAY_FRAME } " << *(frame.goaway_frame);
      break;
    }
    case WINDOW_UPDATE_FRAME: {
      os << "type { WINDOW_UPDATE_FRAME } " << *(frame.window_update_frame);
      break;
    }
    case BLOCKED_FRAME: {
      os << "type { BLOCKED_FRAME } " << *(frame.blocked_frame);
      break;
    }
    case STREAM_FRAME: {
      os << "type { STREAM_FRAME } " << *(frame.stream_frame);
      break;
    }
    case ACK_FRAME: {
      os << "type { ACK_FRAME } " << *(frame.ack_frame);
      break;
    }
    case STOP_WAITING_FRAME: {
      os << "type { STOP_WAITING_FRAME } " << *(frame.stop_waiting_frame);
      break;
    }
    case PING_FRAME: {
      os << "type { PING_FRAME } ";
      break;
    }
    case MTU_DISCOVERY_FRAME: {
      os << "type { MTU_DISCOVERY_FRAME } ";
      break;
    }
    case PATH_CLOSE_FRAME: {
      os << "type { PATH_CLOSE_FRAME } " << *(frame.path_close_frame);
      break;
    }
    default: {
      LOG(ERROR) << "Unknown frame type: " << frame.type;
      break;
    }
  }
  return os;
}

ostream& operator<<(ostream& os, const QuicPaddingFrame& padding_frame) {
  os << "{ num_padding_bytes: " << padding_frame.num_padding_bytes << " }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicRstStreamFrame& rst_frame) {
  os << "{ stream_id: " << rst_frame.stream_id
     << ", error_code: " << rst_frame.error_code << " }\n";
  return os;
}

ostream& operator<<(ostream& os,
                    const QuicConnectionCloseFrame& connection_close_frame) {
  os << "{ error_code: " << connection_close_frame.error_code
     << ", error_details: '" << connection_close_frame.error_details << "' }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicGoAwayFrame& goaway_frame) {
  os << "{ error_code: " << goaway_frame.error_code
     << ", last_good_stream_id: " << goaway_frame.last_good_stream_id
     << ", reason_phrase: '" << goaway_frame.reason_phrase << "' }\n";
  return os;
}

ostream& operator<<(ostream& os,
                    const QuicWindowUpdateFrame& window_update_frame) {
  os << "{ stream_id: " << window_update_frame.stream_id
     << ", byte_offset: " << window_update_frame.byte_offset << " }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicBlockedFrame& blocked_frame) {
  os << "{ stream_id: " << blocked_frame.stream_id << " }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicPathCloseFrame& path_close_frame) {
  os << "{ path_id: " << static_cast<int>(path_close_frame.path_id) << " }\n";
  return os;
}

ostream& operator<<(ostream& os, const QuicStreamFrame& stream_frame) {
  os << "{ stream_id: " << stream_frame.stream_id
     << ", fin: " << stream_frame.fin << ", offset: " << stream_frame.offset
     << ", length: " << stream_frame.data_length << " }\n";
  return os;
}

QuicGoAwayFrame::QuicGoAwayFrame()
    : error_code(QUIC_NO_ERROR), last_good_stream_id(0) {}

QuicGoAwayFrame::QuicGoAwayFrame(QuicErrorCode error_code,
                                 QuicStreamId last_good_stream_id,
                                 const string& reason)
    : error_code(error_code),
      last_good_stream_id(last_good_stream_id),
      reason_phrase(reason) {}

QuicData::QuicData(const char* buffer, size_t length)
    : buffer_(buffer), length_(length), owns_buffer_(false) {}

QuicData::QuicData(const char* buffer, size_t length, bool owns_buffer)
    : buffer_(buffer), length_(length), owns_buffer_(owns_buffer) {}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete[] const_cast<char*>(buffer_);
  }
}

QuicWindowUpdateFrame::QuicWindowUpdateFrame(QuicStreamId stream_id,
                                             QuicStreamOffset byte_offset)
    : stream_id(stream_id), byte_offset(byte_offset) {}

QuicBlockedFrame::QuicBlockedFrame(QuicStreamId stream_id)
    : stream_id(stream_id) {}

QuicPathCloseFrame::QuicPathCloseFrame(QuicPathId path_id) : path_id(path_id) {}

QuicPacket::QuicPacket(char* buffer,
                       size_t length,
                       bool owns_buffer,
                       QuicConnectionIdLength connection_id_length,
                       bool includes_version,
                       bool includes_path_id,
                       bool includes_diversification_nonce,
                       QuicPacketNumberLength packet_number_length)
    : QuicData(buffer, length, owns_buffer),
      buffer_(buffer),
      connection_id_length_(connection_id_length),
      includes_version_(includes_version),
      includes_path_id_(includes_path_id),
      includes_diversification_nonce_(includes_diversification_nonce),
      packet_number_length_(packet_number_length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer, size_t length)
    : QuicData(buffer, length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer,
                                         size_t length,
                                         bool owns_buffer)
    : QuicData(buffer, length, owns_buffer) {}

std::unique_ptr<QuicEncryptedPacket> QuicEncryptedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return base::MakeUnique<QuicEncryptedPacket>(buffer, this->length(), true);
}

ostream& operator<<(ostream& os, const QuicEncryptedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time)
    : QuicEncryptedPacket(buffer, length),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer,
                                       int ttl,
                                       bool ttl_valid)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(ttl_valid ? ttl : -1) {}

std::unique_ptr<QuicReceivedPacket> QuicReceivedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return base::MakeUnique<QuicReceivedPacket>(
      buffer, this->length(), receipt_time(), true, ttl(), ttl() >= 0);
}

ostream& operator<<(ostream& os, const QuicReceivedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

StringPiece QuicPacket::AssociatedData(QuicVersion version) const {
  return StringPiece(
      data(), GetStartOfEncryptedData(version, connection_id_length_,
                                      includes_version_, includes_path_id_,
                                      includes_diversification_nonce_,
                                      packet_number_length_));
}

StringPiece QuicPacket::Plaintext(QuicVersion version) const {
  const size_t start_of_encrypted_data = GetStartOfEncryptedData(
      version, connection_id_length_, includes_version_, includes_path_id_,
      includes_diversification_nonce_, packet_number_length_);
  return StringPiece(data() + start_of_encrypted_data,
                     length() - start_of_encrypted_data);
}

QuicVersionManager::QuicVersionManager(QuicVersionVector supported_versions)
    : enable_version_36_(FLAGS_quic_enable_version_36_v3),
      allowed_supported_versions_(supported_versions),
      filtered_supported_versions_(
          FilterSupportedVersions(supported_versions)) {}

QuicVersionManager::~QuicVersionManager() {}

const QuicVersionVector& QuicVersionManager::GetSupportedVersions() {
  MaybeRefilterSupportedVersions();
  return filtered_supported_versions_;
}

void QuicVersionManager::MaybeRefilterSupportedVersions() {
  if (enable_version_36_ != FLAGS_quic_enable_version_36_v3) {
    enable_version_36_ = FLAGS_quic_enable_version_36_v3;
    RefilterSupportedVersions();
  }
}

void QuicVersionManager::RefilterSupportedVersions() {
  filtered_supported_versions_ =
      FilterSupportedVersions(allowed_supported_versions_);
}

AckListenerWrapper::AckListenerWrapper(QuicAckListenerInterface* listener,
                                       QuicPacketLength data_length)
    : ack_listener(listener), length(data_length) {
  DCHECK(listener != nullptr);
}

AckListenerWrapper::AckListenerWrapper(const AckListenerWrapper& other) =
    default;

AckListenerWrapper::~AckListenerWrapper() {}

SerializedPacket::SerializedPacket(QuicPathId path_id,
                                   QuicPacketNumber packet_number,
                                   QuicPacketNumberLength packet_number_length,
                                   const char* encrypted_buffer,
                                   QuicPacketLength encrypted_length,
                                   bool has_ack,
                                   bool has_stop_waiting)
    : encrypted_buffer(encrypted_buffer),
      encrypted_length(encrypted_length),
      has_crypto_handshake(NOT_HANDSHAKE),
      num_padding_bytes(0),
      path_id(path_id),
      packet_number(packet_number),
      packet_number_length(packet_number_length),
      encryption_level(ENCRYPTION_NONE),
      has_ack(has_ack),
      has_stop_waiting(has_stop_waiting),
      transmission_type(NOT_RETRANSMISSION),
      original_path_id(kInvalidPathId),
      original_packet_number(0) {}

SerializedPacket::SerializedPacket(const SerializedPacket& other) = default;

SerializedPacket::~SerializedPacket() {}

TransmissionInfo::TransmissionInfo()
    : encryption_level(ENCRYPTION_NONE),
      packet_number_length(PACKET_1BYTE_PACKET_NUMBER),
      bytes_sent(0),
      sent_time(QuicTime::Zero()),
      transmission_type(NOT_RETRANSMISSION),
      in_flight(false),
      is_unackable(false),
      has_crypto_handshake(false),
      num_padding_bytes(0),
      retransmission(0) {}

void ClearSerializedPacket(SerializedPacket* serialized_packet) {
  if (!serialized_packet->retransmittable_frames.empty()) {
    DeleteFrames(&serialized_packet->retransmittable_frames);
  }
  serialized_packet->encrypted_buffer = nullptr;
  serialized_packet->encrypted_length = 0;
}

char* CopyBuffer(const SerializedPacket& packet) {
  char* dst_buffer = new char[packet.encrypted_length];
  memcpy(dst_buffer, packet.encrypted_buffer, packet.encrypted_length);
  return dst_buffer;
}

TransmissionInfo::TransmissionInfo(EncryptionLevel level,
                                   QuicPacketNumberLength packet_number_length,
                                   TransmissionType transmission_type,
                                   QuicTime sent_time,
                                   QuicPacketLength bytes_sent,
                                   bool has_crypto_handshake,
                                   int num_padding_bytes)
    : encryption_level(level),
      packet_number_length(packet_number_length),
      bytes_sent(bytes_sent),
      sent_time(sent_time),
      transmission_type(transmission_type),
      in_flight(false),
      is_unackable(false),
      has_crypto_handshake(has_crypto_handshake),
      num_padding_bytes(num_padding_bytes),
      retransmission(0) {}

TransmissionInfo::TransmissionInfo(const TransmissionInfo& other) = default;

TransmissionInfo::~TransmissionInfo() {}

}  // namespace net
