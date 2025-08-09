#pragma once

#include "common/types.hpp"
#include <string_view>
#include <array>
#include <cstdint>

namespace trading {

/// Zero-copy FIX protocol parser.
/// All field values are string_view references into the original message buffer.
/// Common tags (<128) are stored in a flat array for O(1) lookup.
class FixParser {
public:
    static constexpr size_t MAX_COMMON_TAGS = 128;
    static constexpr char DELIMITER = '|'; // SOH replacement for readability

    /// Parse a FIX message. Returns true on success.
    bool parse(std::string_view message) noexcept;

    /// Get field value by tag number. Returns empty string_view if not found.
    std::string_view get_field(int tag) const noexcept;

    /// Convenience: message type (tag 35)
    std::string_view msg_type() const noexcept { return get_field(35); }

    /// Convenience: extract common fields as native types
    OrderId get_order_id() const noexcept;         // Tag 11 (ClOrdID)
    std::string_view get_symbol() const noexcept;   // Tag 55
    Side get_side() const noexcept;                 // Tag 54
    Price get_price() const noexcept;               // Tag 44
    Quantity get_quantity() const noexcept;          // Tag 38
    OrderType get_order_type() const noexcept;      // Tag 40

    /// Market data fields
    Price get_bid_price() const noexcept;           // Tag 132
    Price get_ask_price() const noexcept;           // Tag 133
    Quantity get_bid_size() const noexcept;          // Tag 134
    Quantity get_ask_size() const noexcept;          // Tag 135

    /// Reset parser state for reuse.
    void reset() noexcept;

    /// Is the last parse valid?
    bool valid() const noexcept { return valid_; }

private:
    Price parse_price_field(std::string_view sv) const noexcept;
    uint64_t parse_uint64_field(std::string_view sv) const noexcept;

    // O(1) lookup for common tags (tag < 128)
    std::array<std::string_view, MAX_COMMON_TAGS> common_fields_{};

    // For tags >= 128, we use a simple linear scan of stored entries
    static constexpr size_t MAX_EXTRA_FIELDS = 32;
    struct ExtraField {
        int tag;
        std::string_view value;
    };
    std::array<ExtraField, MAX_EXTRA_FIELDS> extra_fields_{};
    size_t extra_field_count_ = 0;

    bool valid_ = false;
};

} // namespace trading
