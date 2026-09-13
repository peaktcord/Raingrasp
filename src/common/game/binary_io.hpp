#ifndef COMMON_GAME_BINARY_IO_HPP
#define COMMON_GAME_BINARY_IO_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "src/common/runtime.hpp"

class BinaryReader {
public:
    explicit BinaryReader(std::vector<uint8_t> data) : bytes_(std::move(data)) {}
    explicit BinaryReader(const SharedArray<int8_t> &data)
        : BinaryReader(data, 0, data.length()) {}
    BinaryReader(const SharedArray<int8_t> &data, int32_t offset, int32_t length) {
        bytes_.reserve((size_t)length);
        for (int32_t i = 0; i < length; ++i) {
            bytes_.push_back((uint8_t)data[offset + i]);
        }
    }

    int32_t read() {
        if (position_ >= bytes_.size()) return -1;
        return (int32_t)bytes_[position_++];
    }
    int32_t read(SharedArray<int8_t> &out) {
        if (position_ >= bytes_.size()) return -1;
        size_t count = bytes_.size() - position_;
        if (count > (size_t)out.length()) count = (size_t)out.length();
        for (size_t i = 0; i < count; ++i) out[(int32_t)i] = (int8_t)bytes_[position_ + i];
        position_ += count;
        return (int32_t)count;
    }
    int32_t available() const { return (int32_t)(bytes_.size() - position_); }
    void rewind() { position_ = 0; }

    int8_t readByte() {
        int32_t byte = read();
        if (byte < 0) throw std::runtime_error("EOFException");
        return (int8_t)byte;
    }
    bool readBoolean() { return readByte() != 0; }
    int16_t readShort() {
        uint32_t value = ((uint32_t)(uint8_t)readByte() << 8) |
                         (uint32_t)(uint8_t)readByte();
        return (int16_t)value;
    }
    int32_t readInt() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value = (value << 8) | (uint8_t)readByte();
        return (int32_t)value;
    }
    int64_t readLong() {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) value = (value << 8) | (uint8_t)readByte();
        return (int64_t)value;
    }
    std::string readUTF() {
        uint16_t length = (uint16_t)readShort();
        std::string text;
        text.reserve(length);
        for (uint16_t i = 0; i < length; ++i) text += (char)readByte();
        return std::string(text);
    }

private:
    std::vector<uint8_t> bytes_;
    size_t position_ = 0;
};

class BinaryWriter {
public:
    explicit BinaryWriter(int32_t capacity = 0) {
        if (capacity > 0) bytes_.reserve((size_t)capacity);
    }

    void write(int32_t value) { bytes_.push_back((uint8_t)value); }
    void writeByte(int32_t value) { write(value & 0xFF); }
    void writeBoolean(bool value) { writeByte(value ? 1 : 0); }
    void writeShort(int32_t value) {
        uint32_t bits = (uint32_t)value;
        writeByte((int32_t)(bits >> 8));
        writeByte((int32_t)bits);
    }
    void writeInt(int32_t value) {
        uint32_t bits = (uint32_t)value;
        for (int shift = 24; shift >= 0; shift -= 8) writeByte((int32_t)(bits >> shift));
    }
    void writeLong(int64_t value) {
        uint64_t bits = (uint64_t)value;
        for (int shift = 56; shift >= 0; shift -= 8) writeByte((int32_t)(bits >> shift));
    }
    void writeUTF(const std::string &text) {
        writeShort((int32_t)text.size());
        for (unsigned char byte : text) writeByte(byte);
    }
    SharedArray<int8_t> toByteArray() const {
        SharedArray<int8_t> out((int32_t)bytes_.size());
        for (int32_t i = 0; i < out.length(); ++i) out[i] = (int8_t)bytes_[(size_t)i];
        return out;
    }

private:
    std::vector<uint8_t> bytes_;
};

#endif
