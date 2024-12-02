This project implements a real-time market data handler and order management system using C++ with multithreading and networking. 
It uses a UDP socket to receive market data from an exchange and a TCP socket to send processed order data back.
The MarketDataHandler class parses incoming JSON data, updates the OrderBook, and manages buy and sell orders.
Multithreading ensures efficient concurrency, with separate threads handling market data processing and order book operations. 
Synchronization via mutexes ensures thread-safe access to shared resources like the order book.
The project leverages the JsonCpp library for parsing and manipulating JSON data. 
The Winsock library facilitates socket communication, linked via #pragma comment(lib, "Ws2_32.lib"). 
Infinite loops with termination flags enable continuous data handling. The modular design makes the system scalable, responsive, and ideal for high-frequency trading or market data processing applications.






