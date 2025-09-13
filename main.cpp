#include "Orderbook.h"

#include <iostream>
using namespace std;

int main() 
{
    Orderbook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    // auto order = std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10);
    // std::cout << "Order side: " << static_cast<int>(order->GetSide()) << "\n";
    // std::cout << "Order price: " << order->GetPrice() << "\n";
    // std::cout << "Order qty: " << order->GetRemainingQuantity() << "\n";

    // orderbook.AddOrder(order);

    cout << orderbook.Size() << endl;
    orderbook.CancelOrder(orderId);
    std::cout << orderbook.Size() << std::endl;
    return 0;
}