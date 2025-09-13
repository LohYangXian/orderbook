#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"


class Orderbook
{
private:

    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std:: greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    /**
     * @brief Determines if an order can be matched based on the given side and price.
     *
     * For a Buy order, checks if there are any asks and if the specified price is greater than or equal to the best ask price.
     * For a Sell order, checks if there are any bids and if the specified price is less than or equal to the best bid price.
     *
     * @param side The side of the order (Buy or Sell).
     * @param price The price at which to attempt the match.
     * @return true if the order can be matched; false otherwise.
     */
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();

public:

    Orderbook();
    Orderbook(const Orderbook&) = delete;
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;
    ~Orderbook();
    
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetLevelInfos() const;
};