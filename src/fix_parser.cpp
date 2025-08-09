#include "market_data/fix_parser.hpp"
#include <cstring>

namespace trading {

bool FixParser::parse(std::string_view message) noexcept {
    reset();

    if (message.empty()) {
        return false;
    }

    size_t pos = 0;
    while (pos < message.size()) {
        // Find '=' separator
        size_t eq_pos = message.find('=', pos);
        if (eq_pos == std::string_view::npos) break;

        // Parse tag number
        int tag = 0;
        for (size_t i = pos; i < eq_pos; ++i) {
            char c = message[i];
            if (c < '0' || c > '9') { valid_ = false; return false; }
            tag = tag * 10 + (c - '0');
        }

        // Find field delimiter (| or SOH)
        size_t delim_pos = message.find(DELIMITER, eq_pos + 1);
        if (delim_pos == std::string_view::npos) {
            delim_pos = message.size();
        }

        std::string_view value = message.substr(eq_pos + 1, delim_pos - eq_pos - 1);

        // Store field
        if (tag > 0 && tag < static_cast<int>(MAX_COMMON_TAGS)) {
            common_fields_[static_cast<size_t>(tag)] = value;
        } else if (extra_field_count_ < MAX_EXTRA_FIELDS) {
            extra_fields_[extra_field_count_++] = {tag, value};
        }

        pos = delim_pos + 1;
    }

    valid_ = !msg_type().empty();
    return valid_;
}

std::string_view FixParser::get_field(int tag) const noexcept {
    if (tag > 0 && tag < static_cast<int>(MAX_COMMON_TAGS)) {
        return common_fields_[static_cast<size_t>(tag)];
    }
    for (size_t i = 0; i < extra_field_count_; ++i) {
        if (extra_fields_[i].tag == tag) {
            return extra_fields_[i].value;
        }
    }
    return {};
}

void FixParser::reset() noexcept {
    for (size_t i = 0; i < MAX_COMMON_TAGS; ++i) {
        common_fields_[i] = {};
    }
    extra_field_count_ = 0;
    valid_ = false;
}

Price FixParser::parse_price_field(std::string_view sv) const noexcept {
    if (sv.empty()) return 0;

    bool negative = false;
    size_t i = 0;
    if (sv[0] == '-') { negative = true; ++i; }

    int64_t integer_part = 0;
    int64_t decimal_part = 0;
    int decimal_digits = 0;
    bool in_decimal = false;

    for (; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '.') {
            in_decimal = true;
            continue;
        }
        if (c < '0' || c > '9') break;

        if (in_decimal) {
            if (decimal_digits < 2) { // Only 2 decimal places for price
                decimal_part = decimal_part * 10 + (c - '0');
                ++decimal_digits;
            }
        } else {
            integer_part = integer_part * 10 + (c - '0');
        }
    }

    // Pad decimal to 2 digits
    while (decimal_digits < 2) {
        decimal_part *= 10;
        ++decimal_digits;
    }

    Price result = integer_part * PRICE_SCALE + decimal_part;
    return negative ? -result : result;
}

uint64_t FixParser::parse_uint64_field(std::string_view sv) const noexcept {
    if (sv.empty()) return 0;
    uint64_t result = 0;
    for (char c : sv) {
        if (c < '0' || c > '9') break;
        result = result * 10 + static_cast<uint64_t>(c - '0');
    }
    return result;
}

OrderId FixParser::get_order_id() const noexcept {
    std::string_view sv = get_field(11);
    return parse_uint64_field(sv);
}

std::string_view FixParser::get_symbol() const noexcept {
    return get_field(55);
}

Side FixParser::get_side() const noexcept {
    std::string_view sv = get_field(54);
    if (sv == "1") return Side::Buy;
    return Side::Sell;
}

Price FixParser::get_price() const noexcept {
    return parse_price_field(get_field(44));
}

Quantity FixParser::get_quantity() const noexcept {
    return parse_uint64_field(get_field(38));
}

OrderType FixParser::get_order_type() const noexcept {
    std::string_view sv = get_field(40);
    if (sv == "1") return OrderType::Market;
    if (sv == "2") return OrderType::Limit;
    if (sv == "3") return OrderType::IOC;
    if (sv == "4") return OrderType::FOK;
    return OrderType::Limit;
}

Price FixParser::get_bid_price() const noexcept {
    return parse_price_field(get_field(132));
}

Price FixParser::get_ask_price() const noexcept {
    return parse_price_field(get_field(133));
}

Quantity FixParser::get_bid_size() const noexcept {
    return parse_uint64_field(get_field(134));
}

Quantity FixParser::get_ask_size() const noexcept {
    return parse_uint64_field(get_field(135));
}

} // namespace trading
