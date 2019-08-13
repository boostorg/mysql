#ifndef INCLUDE_MESSAGE_SERIALIZATION_HPP_
#define INCLUDE_MESSAGE_SERIALIZATION_HPP_

#include "basic_types.hpp"
#include "messages.hpp"
#include "basic_serialization.hpp"
#include <iosfwd>

namespace mysql
{

// general
ReadIterator deserialize(ReadIterator from, ReadIterator last, PacketHeader& output);
void serialize(DynamicBuffer& buffer, const PacketHeader& value);
ReadIterator deserialize(ReadIterator from, ReadIterator last, OkPacket& output);
ReadIterator deserialize(ReadIterator from, ReadIterator last, ErrPacket& output);

// Connection phase
ReadIterator deserialize(ReadIterator from, ReadIterator last, Handshake& output);
void serialize(DynamicBuffer& buffer, const HandshakeResponse& value);

// Resultsets
ReadIterator deserialize(ReadIterator from, ReadIterator last, ColumnDefinition& output);
void serialize_binary_value(DynamicBuffer& buffer, const BinaryValue& value);

// Prepared statements
void serialize(DynamicBuffer& buffer, const StmtPrepare& value);
ReadIterator deserialize(ReadIterator from, ReadIterator last, StmtPrepareResponseHeader& output);
void serialize(DynamicBuffer& buffer, const StmtExecute& value);
ReadIterator deserialize(ReadIterator from, ReadIterator last, StmtExecuteResponseHeader& output);
std::pair<FieldType, bool> compute_field_type(const BinaryValue&);

// Text serialization
std::ostream& operator<<(std::ostream& os, const Handshake& value);
std::ostream& operator<<(std::ostream& os, const HandshakeResponse& value);

}



#endif /* INCLUDE_MESSAGE_SERIALIZATION_HPP_ */
