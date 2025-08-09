#include <gtest/gtest.h>
#include "market_data/market_data_handler.hpp"

using namespace trading;

TEST(MarketDataHandlerTest, ProcessSnapshot) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);

    std::string msg = "8=FIX.4.4|9=200|35=W|49=FEED|56=CLIENT|34=1|"
                      "55=AAPL|132=150.00|133=150.50|134=100|135=200|44=150.25|38=50|10=000|";

    EXPECT_TRUE(handler.process_message(msg));
    EXPECT_EQ(handler.messages_processed(), 1u);

    MarketDataMessage md;
    EXPECT_TRUE(queue.try_pop(md));
    EXPECT_EQ(md.msg_type, 'W');
    EXPECT_EQ(md.instrument, 0u); // AAPL = 0
    EXPECT_EQ(md.bid_price, 15000);
    EXPECT_EQ(md.ask_price, 15050);
    EXPECT_EQ(md.bid_quantity, 100u);
    EXPECT_EQ(md.ask_quantity, 200u);
}

TEST(MarketDataHandlerTest, OrderPreserved) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);

    for (int i = 0; i < 10; ++i) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "8=FIX.4.4|35=W|55=AAPL|132=%.2f|133=%.2f|134=100|135=100|44=%.2f|38=50|10=000|",
            100.0 + i, 100.5 + i, 100.25 + i);
        handler.process_message(msg);
    }

    // Verify ordering
    for (int i = 0; i < 10; ++i) {
        MarketDataMessage md;
        EXPECT_TRUE(queue.try_pop(md));
        EXPECT_EQ(md.bid_price, 10000 + i * 100);
    }
}

TEST(MarketDataHandlerTest, UnsupportedMessageType) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);

    std::string msg = "8=FIX.4.4|35=A|49=CLIENT|56=EXCHANGE|"; // Logon
    EXPECT_FALSE(handler.process_message(msg));
}

TEST(MarketDataHandlerTest, SymbolMapping) {
    EXPECT_EQ(MarketDataHandler::symbol_to_id("AAPL"), 0u);
    EXPECT_EQ(MarketDataHandler::symbol_to_id("GOOG"), 1u);
    EXPECT_EQ(MarketDataHandler::symbol_to_id("MSFT"), 2u);
}

TEST(MarketDataHandlerTest, InvalidMessage) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);
    EXPECT_FALSE(handler.process_message("garbage"));
    EXPECT_EQ(handler.messages_processed(), 0u);
}
