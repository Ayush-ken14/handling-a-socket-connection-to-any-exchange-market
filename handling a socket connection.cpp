#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>
#include <json/json.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
// Constants
#define UDP_PORT 8081
#define TCP_PORT 8080
#define BUFFER_SIZE 1024

// Order structure
struct Order
{
    int orderId;
    double price;
    int quantity;

    Order(int id, double p, int q) : orderId(id), price(p), quantity(q) {}
};

// OrderBook class with mutex
class OrderBook
{
private:
    std::map<int, Order> buyOrders;  // Buy orders map
    std::map<int, Order> sellOrders; // Sell orders map
    std::mutex mtx;                  // Mutex for thread-safe access

public:
    void addBuyOrder(const Order &order)
    {
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        buyOrders[order.orderId] = order;
        std::cout << "Buy Order added: ID=" << order.orderId
                  << ", Price=" << order.price
                  << ", Quantity=" << order.quantity << "\n";
    }

    void addSellOrder(const Order &order)
    {
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        sellOrders[order.orderId] = order;
        std::cout << "Sell Order added: ID=" << order.orderId
                  << ", Price=" << order.price
                  << ", Quantity=" << order.quantity << "\n";
    }

    void processBuyOrders()
    {
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        for (const auto &[id, order] : buyOrders)
        {
            std::cout << "Processing Buy Order: ID=" << id
                      << ", Price=" << order.price
                      << ", Quantity=" << order.quantity << "\n";
        }
    }

    void processSellOrders()
    {
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        for (const auto &[id, order] : sellOrders)
        {
            std::cout << "Processing Sell Order: ID=" << id
                      << ", Price=" << order.price
                      << ", Quantity=" << order.quantity << "\n";
        }
    }
};

// SocketConnection class
class SocketConnection
{
private:
    SOCKET udpSocket;
    SOCKET tcpSocket;
    sockaddr_in udpAddr, tcpAddr;
    bool tcpConnected = false;

public:
    SocketConnection(const std::string &udpIp, const std::string &tcpIp)
    {
        WSAData wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Setup UDP socket
        udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        udpAddr.sin_family = AF_INET;
        udpAddr.sin_port = htons(UDP_PORT);
        inet_pton(AF_INET, udpIp.c_str(), &udpAddr.sin_addr);

        // Setup TCP socket
        tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
        tcpAddr.sin_family = AF_INET;
        tcpAddr.sin_port = htons(TCP_PORT);
        inet_pton(AF_INET, tcpIp.c_str(), &tcpAddr.sin_addr);
    }

    bool connectTcp()
    {
        tcpConnected = (connect(tcpSocket, (struct sockaddr *)&tcpAddr, sizeof(tcpAddr)) == 0);
        if (tcpConnected)
        {
            std::cout << "Connected to exchange via TCP.\n";
        }
        else
        {
            std::cerr << "Failed to connect to exchange via TCP.\n";
        }
        return tcpConnected;
    }

    void disconnectTcp()
    {
        closesocket(tcpSocket);
        tcpConnected = false;
    }

    bool isTcpConnected() const
    {
        return tcpConnected;
    }

    int receiveUdp(char *buffer, int bufferSize)
    {
        sockaddr_in from;
        int fromLen = sizeof(from);
        return recvfrom(udpSocket, buffer, bufferSize, 0, (sockaddr *)&from, &fromLen);
    }

    void sendTcp(const std::string &data)
    {
        if (tcpConnected)
        {
            send(tcpSocket, data.c_str(), data.length(), 0);
        }
        else
        {
            std::cerr << "TCP connection is not established.\n";
        }
    }

    ~SocketConnection()
    {
        closesocket(udpSocket);
        closesocket(tcpSocket);
        WSACleanup();
    }
};

// MarketDataHandler class
class MarketDataHandler
{
private:
    SocketConnection &socketConnection;
    OrderBook &orderBook;

public:
    MarketDataHandler(SocketConnection &socket, OrderBook &book)
        : socketConnection(socket), orderBook(book) {}

    void processMarketData(const std::string &data)
    {
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errors;

        if (Json::parseFromStream(reader, std::istringstream(data), &root, &errors))
        {
            int orderId = root["order_id"].asInt();
            double price = root["price"].asDouble();
            int quantity = root["quantity"].asInt();
            std::string action = root["action"].asString();
            std::string orderType = root["type"].asString();

            if (orderType == "buy")
            {
                if (action == "add")
                {
                    orderBook.addBuyOrder(Order(orderId, price, quantity));
                }
            }
            else if (orderType == "sell")
            {
                if (action == "add")
                {
                    orderBook.addSellOrder(Order(orderId, price, quantity));
                }
            }
        }
        else
        {
            std::cerr << "Failed to parse market data: " << errors << "\n";
        }
    }

    void handleDataStream()
    {
        char buffer[BUFFER_SIZE];
        while (true)
        {
            // Receive UDP data
            int bytesReceived = socketConnection.receiveUdp(buffer, BUFFER_SIZE);
            if (bytesReceived > 0)
            {
                std::string data(buffer, bytesReceived);
                processMarketData(data);
            }
            else
            {
                std::cerr << "Error receiving data over UDP.\n";
            }
        }
    }
};

// Main function
int main()
{
    const std::string udpIp = "127.0.0.1";
    const std::string tcpIp = "127.0.0.1";

    SocketConnection socket(udpIp, tcpIp);
    OrderBook orderBook;
    MarketDataHandler handler(socket, orderBook);

    if (socket.connectTcp())
    {
        // Create threads for buy and sell processing
        std::thread buyThread(&OrderBook::processBuyOrders, &orderBook);
        std::thread sellThread(&OrderBook::processSellOrders, &orderBook);

        // Create thread for data handling
        std::thread dataStreamThread(&MarketDataHandler::handleDataStream, &handler);

        // Join threads
        buyThread.join();
        sellThread.join();
        dataStreamThread.join();
    }

    return 0;
}
