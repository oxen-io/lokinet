#pragma once

#include "question.hpp"
#include "rr.hpp"
#include "serialize.hpp"

#include <optional>

namespace llarp
{
    struct IPPacket;

    namespace dns
    {
        struct SRVData;

        struct MessageHeader : public Serialize
        {
          public:
            static constexpr size_t Size = 12;

            MessageHeader() = default;

            uint16_t _id;
            uint16_t _fields;
            uint16_t _qd_count;
            uint16_t _an_count;
            uint16_t _ns_count;
            uint16_t _ar_count;

            bool Encode(llarp_buffer_t* buf) const override;

            bool Decode(llarp_buffer_t* buf) override;

            nlohmann::json ToJSON() const override;

            bool operator==(const MessageHeader& h) const
            {
                return std::tie(_id, _fields, _qd_count, _an_count, _ns_count, _ar_count)
                    == std::tie(h._id, h._fields, h._qd_count, h._an_count, h._ns_count, h._ar_count);
            }
        };

        struct Message : public Serialize
        {
            explicit Message(const MessageHeader& hdr);
            explicit Message(const Question& question);

            Message(Message&& other);
            Message(const Message& other);

            nlohmann::json ToJSON() const override;

            void add_nx_reply(RR_TTL_t ttl = 1);

            void add_serv_fail(RR_TTL_t ttl = 30);

            void add_mx_reply(std::string name, uint16_t priority, RR_TTL_t ttl = 1);

            void add_CNAME_reply(std::string name, RR_TTL_t ttl = 1);

            void add_IN_reply(uint32_t addr, RR_TTL_t ttl = 1);

            void add_reply(std::string name, RR_TTL_t ttl = 1);

            void add_srv_reply(std::vector<SRVData> records, RR_TTL_t ttl = 1);

            void add_ns_reply(std::string name, RR_TTL_t ttl = 1);

            void add_txt_reply(std::string value, RR_TTL_t ttl = 1);

            bool Encode(llarp_buffer_t* buf) const override;

            bool Decode(llarp_buffer_t* buf) override;

            // Wrapper around Encode that encodes into a new buffer and returns it
            std::vector<std::byte> to_buffer() const;

            std::string to_string() const;

            uint16_t hdr_id;
            uint16_t hdr_fields;
            std::vector<Question> questions;
            std::vector<ResourceRecord> answers;
            std::vector<ResourceRecord> authorities;
            std::vector<ResourceRecord> additional;
        };

        std::optional<Message> maybe_parse_dns_msg(std::span<const std::byte> buf);
    }  // namespace dns

}  // namespace llarp
