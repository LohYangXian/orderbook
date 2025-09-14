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

    struct LevelData
    {
        Quantity quantity_{ };
        Quantity count_{ };

        enum class Action
        {
            Add,
            Remove,
            Match
        };
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std:: greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{ false };


    void PruneGoodForDayOrders();

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;

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

    /**
     * @brief Default constructor for Orderbook.
     *
     * Initializes the order book and any background resources
     * (e.g., threads, data structures).
     */
    Orderbook();

    /**
     * @brief Copy constructor (deleted).
     *
     * Prevents copying of the Orderbook object.
     * Copying is disabled because the class manages resources
     * such as threads, mutexes, and condition variables
     * that cannot be safely duplicated.
     */
    Orderbook(const Orderbook&) = delete;

    /**
     * @brief Copy assignment operator (deleted).
     *
     * Prevents assigning one Orderbook to another.
     * Disabled for the same reasons as the copy constructor.
     */
    void operator=(const Orderbook&) = delete;

    /**
     * @brief Move constructor (deleted).
     *
     * Prevents moving of the Orderbook object.
     * Moving is disabled because transferring ownership of
     * active threads, mutexes, and synchronization primitives
     * could lead to undefined behavior.
     */
    Orderbook(Orderbook&&) = delete;

    /**
     * @brief Move assignment operator (deleted).
     *
     * Prevents move-assignment of the Orderbook object.
     * Disabled to ensure the integrity of resources owned
     * by the class.
     */
    void operator=(Orderbook&&) = delete;

    /**
     * @brief Destructor for Orderbook.
     *
     * Cleans up resources before destruction.
     * Typically ensures the background thread is stopped,
     * the shutdown flag is set, and synchronization objects
     * are properly released.
     */
    ~Orderbook();
    
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;
};