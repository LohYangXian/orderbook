#include "Orderbook.h"

#include <numeric>
#include <chrono>
#include <ctime>


/**
 * @brief Background task to prune (cancel) Good-for-Day orders at end of trading day.
 *
 * This function runs in a loop on a dedicated thread until shutdown.
 * It calculates the next cutoff time (default 16:00 local time),
 * then waits until that point or until a shutdown signal is received.
 *
 * Workflow:
 * 1. Determine current local time.
 * 2. If past cutoff, roll forward to the next day.
 * 3. Compute the exact cutoff (16:00:00) as a time_point.
 * 4. Wait until the cutoff time or until shutdown is triggered.
 * 5. At cutoff, acquire a lock on the order book and collect
 *    all orders marked as OrderType::GoodForDay.
 * 6. Cancel all such orders using CancelOrders().
 * 7. Repeat the loop for the next trading day.
 *
 * Thread safety:
 * - Uses std::mutex and std::condition_variable for synchronization.
 * - Gracefully exits if shutdown_ is set or condition variable is notified.
 *
 * Notes:
 * - Good-for-Day (GFD) orders are only valid until the trading day's end.
 * - This ensures expired orders do not persist into the next session.
 */
void Orderbook::PruneGoodForDayOrders()
{    
    using namespace std::chrono;
    const auto end = hours(16);

	while (true)
	{
		const auto now = system_clock::now();
		const auto now_c = system_clock::to_time_t(now);
		std::tm now_parts;
		localtime_r(&now_c, &now_parts);

		if (now_parts.tm_hour >= end.count())
			now_parts.tm_mday += 1;

		now_parts.tm_hour = end.count();
		now_parts.tm_min = 0;
		now_parts.tm_sec = 0;

		auto next = system_clock::from_time_t(mktime(&now_parts));
		auto till = next - now + milliseconds(100);

		{
			std::unique_lock ordersLock{ ordersMutex_ };

			if (shutdown_.load(std::memory_order_acquire) ||
				shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
				return;
		}

		OrderIds orderIds;

		{
			std::scoped_lock ordersLock{ ordersMutex_ };

			for (const auto& [_, entry] : orders_)
			{
				const auto& [order, _] = entry;

				if (order->GetOrderType() != OrderType::GoodForDay)
					continue;

				orderIds.push_back(order->GetOrderId());
			}
		}

		CancelOrders(orderIds);
	}
}

/**
 * @brief Cancels a batch of orders in a thread-safe manner.
 *
 * This function iterates over a collection of order IDs and cancels each order.
 * Thread safety is ensured by acquiring a lock on the order book mutex
 * for the entire duration of the function.
 *
 * @param orderIds A collection of order IDs to cancel.
 *
 * Details:
 * - The function uses std::scoped_lock to acquire ordersMutex_:
 *     - Locks the mutex upon creation and automatically releases it
 *       when the lock goes out of scope.
 *     - Ensures no other thread can modify the order book concurrently.
 * - Each order ID is passed to CancelOrderInternal(orderId), which:
 *     1. Locates the order in the internal order map.
 *     2. Removes it from the order book data structures.
 *     3. Updates relevant state (e.g., price level totals, best bid/ask).
 *     4. Optionally logs or notifies external systems of the cancellation.
 *
 * Example usage:
 * @code
 * OrderIds ids = {123, 456, 789};
 * orderbook.CancelOrders(ids);
 * @endcode
 */
void Orderbook::CancelOrders(OrderIds orderIds)
{
	std::scoped_lock ordersLock{ ordersMutex_ };

	for (const auto& orderId : orderIds)
		CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId)
{
	if (!orders_.contains(orderId))
		return;

	const auto [order, iterator] = orders_.at(orderId);
	orders_.erase(orderId);

	if (order->GetSide() == Side::Sell)
	{
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			asks_.erase(price);
	}
	else
	{
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			bids_.erase(price);
	}

	OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
	UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
	UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
	UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
	auto& data = data_[price];

	data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;
	if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
	{
		data.quantity_ -= quantity;
	}
	else
	{
		data.quantity_ += quantity;
	}

	if (data.count_ == 0)
		data_.erase(price);
}

/**
 * @brief Checks if an order can be fully filled at the given price and quantity.
 *
 * This function determines whether there is sufficient liquidity on the
 * opposite side of the order book to completely fill an order
 * of the specified quantity at the specified price.
 *
 * Workflow:
 * 1. Checks if there is any matchable price level on the opposite side
 *    using CanMatch(). If not, returns false.
 * 2. Determines the threshold price on the opposite side:
 *      - For Buy orders, the lowest ask price.
 *      - For Sell orders, the highest bid price.
 * 3. Iterates through all price levels in data_:
 *      - Skips levels outside the threshold.
 *      - Skips levels that are worse than the order price.
 *      - If a level has enough quantity to fill the remaining order, returns true.
 *      - Otherwise, subtracts the level's quantity from the remaining order and continues.
 * 4. If the loop finishes without fully filling the order, returns false.
 *
 * @param side The side of the order (Buy or Sell).
 * @param price The limit price of the order.
 * @param quantity The total quantity of the order to fill.
 * @return true if the order can be completely filled, false otherwise.
 *
 * Notes:
 * - This function does not modify the order book.
 * - Used internally to determine whether a new order would be fully executable.
 * - The method is marked const to indicate it does not change class state.
 */
bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
	if (!CanMatch(side, price))
		return false;

	std::optional<Price> threshold;

	if (side == Side::Buy)
	{
		const auto [askPrice, _] = *asks_.begin();
		threshold = askPrice;
	}
	else
	{
		const auto [bidPrice, _] = *bids_.begin();
		threshold = bidPrice;
	}

	for (const auto& [levelPrice, levelData] : data_)
	{
		if (threshold.has_value() &&
			((side == Side::Buy && threshold.value() > levelPrice) ||
			(side == Side::Sell && threshold.value() < levelPrice)))
			continue;

		if ((side == Side::Buy && levelPrice > price) ||
			(side == Side::Sell && levelPrice < price))
			continue;

		if (quantity <= levelData.quantity_)
			return true;

		quantity -= levelData.quantity_;
	}

	return false;
}

bool Orderbook::CanMatch(Side side, Price price) const
{
	if (side == Side::Buy)
	{
		if (asks_.empty())
			return false;

		const auto& [bestAsk, _] = *asks_.begin();
		return price >= bestAsk;
	}
	else
	{
		if (bids_.empty())
			return false;

		const auto& [bestBid, _] = *bids_.begin();
		return price <= bestBid;
	}
}

Trades Orderbook::MatchOrders()
{
	Trades trades;
	trades.reserve(orders_.size());

	while (true)
	{
		if (bids_.empty() || asks_.empty())
			break;

		auto& [bidPrice, bids] = *bids_.begin();
		auto& [askPrice, asks] = *asks_.begin();

		if (bidPrice < askPrice)
			break;

		while (!bids.empty() && !asks.empty())
		{
			auto bid = bids.front();
			auto ask = asks.front();

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

			bid->Fill(quantity);
			ask->Fill(quantity);

			if (bid->IsFilled())
			{
				bids.pop_front();
				orders_.erase(bid->GetOrderId());
			}

			if (ask->IsFilled())
			{
				asks.pop_front();
				orders_.erase(ask->GetOrderId());
			}


			trades.push_back(Trade{
				TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
				TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity } 
				});

			OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
			OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
		}

        if (bids.empty())
        {
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
        }

        if (asks.empty())
        {
            asks_.erase(askPrice);
            data_.erase(askPrice);
        }
	}

	if (!bids_.empty())
	{
		auto& [_, bids] = *bids_.begin();
		auto& order = bids.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	if (!asks_.empty())
	{
		auto& [_, asks] = *asks_.begin();
		auto& order = asks.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	return trades;
}

Orderbook::Orderbook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } } { }

/**
 * @brief Destructor for Orderbook.
 *
 * Gracefully shuts down the order book's background thread.
 *
 * Implementation steps:
 * 1. Set the shutdown_ atomic flag to true.
 * 2. Notify the condition variable to wake up the background thread.
 * 3. Join the thread to wait for its completion before destruction.
 *
 * This ensures that no background thread continues running after
 * the Orderbook object is destroyed, maintaining thread safety.
 */
Orderbook::~Orderbook()
{
    shutdown_.store(true, std::memory_order_release);
	shutdownConditionVariable_.notify_one();
	ordersPruneThread_.join();
}

Trades Orderbook::AddOrder(OrderPointer order)
{
	std::scoped_lock ordersLock{ ordersMutex_ };

	if (orders_.contains(order->GetOrderId()))
		return { };

	if (order->GetOrderType() == OrderType::Market)
	{
		if (order->GetSide() == Side::Buy && !asks_.empty())
		{
			const auto& [worstAsk, _] = *asks_.rbegin();
			order->ToGoodTillCancel(worstAsk);
		}
		else if (order->GetSide() == Side::Sell && !bids_.empty())
		{
			const auto& [worstBid, _] = *bids_.rbegin();
			order->ToGoodTillCancel(worstBid);
		}
		else
			return { };
	}

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
		return { };

	if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
		return { };

	OrderPointers::iterator iterator;

	if (order->GetSide() == Side::Buy)
	{
		auto& orders = bids_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	}
	else
	{
		auto& orders = asks_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	}

	orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
	
	OnOrderAdded(order);
	
	return MatchOrders();

}

void Orderbook::CancelOrder(OrderId orderId)
{
	std::scoped_lock ordersLock{ ordersMutex_ };

	CancelOrderInternal(orderId);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
	OrderType orderType;

	{
		std::scoped_lock ordersLock{ ordersMutex_ };

		if (!orders_.contains(order.GetOrderId()))
			return { };

		const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
		orderType = existingOrder->GetOrderType();
	}

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

std::size_t Orderbook::Size() const
{
	std::scoped_lock ordersLock{ ordersMutex_ };
	return orders_.size(); 
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
	LevelInfos bidInfos, askInfos;
	bidInfos.reserve(orders_.size());
	askInfos.reserve(orders_.size());

	auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
	{
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity runningSum, const OrderPointer& order)
			{ return runningSum + order->GetRemainingQuantity(); }) };
	};

	for (const auto& [price, orders] : bids_)
		bidInfos.push_back(CreateLevelInfos(price, orders));

	for (const auto& [price, orders] : asks_)
		askInfos.push_back(CreateLevelInfos(price, orders));

	return OrderbookLevelInfos{ bidInfos, askInfos };

}
