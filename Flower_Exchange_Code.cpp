//Flower Exchange app by Kodikara U. S. S. 210293K and Sehara G. M. M. 210583B

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <iterator>

using namespace std;

// Order struct to store order details and handle validation and execution
struct Order
{
    string ClientOrderID;
    string Instrument;
    int Side;
    double Price;
    int Quantity;

    string OrderID;
    int Status;  // 0 = valid, 1 = invalid, 2 = fill, 3 = partial fill
    string Reason;

    int RemainingQty;
    int Priority;

    static int CurrentOrderID;
    string Instruments[5] = {"Rose", "Lavender", "Lotus", "Tulip", "Orchid"};

    Order() {}

    // Constructor to initialize an order with provided details
    Order(string* clientOrderID, string* instrument, string* side, string* qty, string* price)
    {
        ClientOrderID = *clientOrderID;
        OrderID = "ord" + to_string(CurrentOrderID++);
        Instrument = *instrument;
        Side = stoi(*side);
        Price = stod(*price);
        Quantity = stoi(*qty);

        validateOrder(clientOrderID, instrument, side, qty, price);

        RemainingQty = Quantity;
        Priority = 1;
    }

    // Function to validate order fields
    void validateOrder(string* clientOrderID, string* instrument, string* side, string* qty, string* price)
    {
        // Check if all fields are non-empty
        if (!((*clientOrderID).length() && (*instrument).length() && (*side).length() && (*price).length() && (*qty).length()))
        {
            Status = 1;
            Reason = "Invalid Fields";
            return;
        }

        // Check if instrument is valid
        if (find(begin(Instruments), end(Instruments), *instrument) == end(Instruments))
        {
            Status = 1;
            Reason = "Invalid Instrument";
            return;
        }

        // Check if side is valid (1 = buy, 2 = sell)
        int s = stoi(*side);
        if (s != 1 && s != 2)
        {
            Status = 1;
            Reason = "Invalid Side";
            return;
        }

        // Check if price is positive
        double p = stod(*price);
        if (!(p > 0.0))
        {
            Status = 1;
            Reason = "Invalid Price";
            return;
        }

        // Check if quantity is a multiple of 10 and within 10-1000
        int q = stoi(*qty);
        if (q % 10 != 0 || q < 10 || q > 1000)
        {
            Status = 1;
            Reason = "Invalid Size";
            return;
        }

        Status = 0;
        Reason = "Accepted";
    }

    // Check if order is valid
    bool isValid()
    {
        return Status != 1;
    }

    // Execute the order and write details to output file
    void execute(ofstream& output, int execQty)
    {
        auto now = chrono::system_clock::now();
        auto nowMs = chrono::time_point_cast<chrono::milliseconds>(now);
        long msCount = nowMs.time_since_epoch().count();

        time_t nowC = chrono::system_clock::to_time_t(now);
        tm *lt = localtime(&nowC);

        string statusStr;
        if (Status == 0) statusStr = "New";
        else if (Status == 1) statusStr = "Reject";
        else if (Status == 2) statusStr = "Fill";
        else if (Status == 3) statusStr = "PFill";

        output << OrderID << "," << ClientOrderID << ","  << Instrument << ","
               << Side << "," << statusStr << "," << execQty << ","  << Price
               << "," << Reason << "," << put_time(lt, "%Y.%m.%d-%H.%M.%S")
               << "." << setfill('0') << setw(3) << msCount % 1000 << "\n";
    }

    // Overload execute function to handle remaining quantity
    void execute(ofstream& output)
    {
        execute(output, RemainingQty);
    }
};

int Order::CurrentOrderID = 1;

// Add order to order book based on price and side (buy/sell)
void addOrderToBook(vector<Order>& orderBook, Order newOrder, int side)
{
    if (orderBook.empty())
    {
        orderBook.push_back(newOrder);
        return;
    }

    auto it = begin(orderBook);

    // Insert order based on price priority
    while (it != end(orderBook))
    {
        if ((it->Price < newOrder.Price && side == 1) || (it->Price > newOrder.Price && side == 2))
        {
            break;
        }
        ++it;
    }

    // Adjust priority for orders with same price
    if (it == end(orderBook))
    {
        orderBook.push_back(newOrder);
    }
    else
    {
        if ((it != begin(orderBook)) && (it - 1)->Price == newOrder.Price)
        {
            newOrder.Priority = ((it - 1)->Priority) + 1;
        }
        orderBook.insert(it, newOrder);
    }
}

// Process all orders from input file
void processOrders(string inputFile, string outputFile)
{
    vector<vector<Order>> OrderBook_Rose(2), OrderBook_Lavender(2), OrderBook_Lotus(2), OrderBook_Tulip(2), OrderBook_Orchid(2);
    vector<vector<Order>> allBooks[5] = {OrderBook_Rose, OrderBook_Lavender, OrderBook_Lotus, OrderBook_Tulip, OrderBook_Orchid};
    string Instruments[5] = {"Rose", "Lavender", "Lotus", "Tulip", "Orchid"};

    ifstream input(inputFile);
    ofstream output(outputFile);  // Use custom output file name
    output << "Order ID,Client Order ID,Instrument,Side,Exec Status,Quantity,Price,Reason,Transaction Time\n";

    string line, word;
    vector<string> row;
    int count = 0;

    while (getline(input, line))
    {
        if (++count == 1) continue;  // Skip the header

        row.clear();
        stringstream s(line);

        while (getline(s, word, ','))
        {
            row.push_back(word);
        }

        Order newOrder(&row[0], &row[1], &row[2], &row[3], &row[4]);

        if (newOrder.isValid())
        {
            int bookIndex = (int)(find(begin(Instruments), end(Instruments), newOrder.Instrument) - begin(Instruments));
            vector<vector<Order>> orderBook = allBooks[bookIndex];

            if (newOrder.Side == 1)  // Buy side
            {
                while (!orderBook[1].empty() && (orderBook[1][0].Price <= newOrder.Price))
                {
                    // Matching logic
                    if (newOrder.RemainingQty == orderBook[1][0].RemainingQty)
                    {
                        newOrder.Status = 2;
                        orderBook[1][0].Status = 2;
                        newOrder.execute(output);
                        orderBook[1][0].execute(output);
                        newOrder.RemainingQty = 0;
                        orderBook[1][0].RemainingQty = 0;
                        orderBook[1].erase(begin(orderBook[1]));
                        break;
                    }
                    else if (newOrder.RemainingQty > orderBook[1][0].RemainingQty)
                    {
                        newOrder.Status = 3;
                        orderBook[1][0].Status = 2;
                        newOrder.execute(output, orderBook[1][0].RemainingQty);
                        orderBook[1][0].execute(output);
                        newOrder.RemainingQty -= orderBook[1][0].RemainingQty;
                        orderBook[1].erase(begin(orderBook[1]));
                    }
                    else
                    {
                        newOrder.Status = 2;
                        orderBook[1][0].Status = 3;
                        newOrder.execute(output);
                        orderBook[1][0].execute(output, newOrder.RemainingQty);
                        orderBook[1][0].RemainingQty -= newOrder.RemainingQty;
                        newOrder.RemainingQty = 0;
                        break;
                    }
                }

                if (newOrder.Status == 0)
                {
                    newOrder.execute(output);
                }

                if (newOrder.RemainingQty > 0.0)
                {
                    addOrderToBook(orderBook[0], newOrder, 1);
                }

            }
            else if (newOrder.Side == 2)  // Sell side
            {
                while (!orderBook[0].empty() && (orderBook[0][0].Price >= newOrder.Price))
                {
                    // Matching logic
                    if (newOrder.RemainingQty == orderBook[0][0].RemainingQty)
                    {
                        newOrder.Status = 2;
                        orderBook[0][0].Status = 2;
                        newOrder.execute(output);
                        orderBook[0][0].execute(output);
                        newOrder.RemainingQty = 0;
                        orderBook[0][0].RemainingQty = 0;
                        orderBook[0].erase(begin(orderBook[0]));
                        break;
                    }
                    else if (newOrder.RemainingQty > orderBook[0][0].RemainingQty)
                    {
                        newOrder.Status = 3;
                        orderBook[0][0].Status = 2;
                        newOrder.execute(output, orderBook[0][0].RemainingQty);
                        orderBook[0][0].execute(output);
                        newOrder.RemainingQty -= orderBook[0][0].RemainingQty;
                        orderBook[0].erase(begin(orderBook[0]));
                    }
                    else
                    {
                        newOrder.Status = 2;
                        orderBook[0][0].Status = 3;
                        newOrder.execute(output);
                        orderBook[0][0].execute(output, newOrder.RemainingQty);
                        orderBook[0][0].RemainingQty -= newOrder.RemainingQty;
                        newOrder.RemainingQty = 0;
                        break;
                    }
                }

                if (newOrder.Status == 0)
                {
                    newOrder.execute(output);
                }

                if (newOrder.RemainingQty > 0.0)
                {
                    addOrderToBook(orderBook[1], newOrder, 2);
                }
            }

            allBooks[bookIndex] = orderBook;
        }
        else
        {
            newOrder.execute(output);
        }
    }

    input.close();
    output.close();
}

int main()
{
    // This for loop process all 8 example input files
    for (int i = 1; i <= 8; i++)
    {
        // Generate input and output file names dynamically
        string inputFile = "./inputs/Example " + to_string(i) + ".csv";
        string outputFile = "./outputs/Example " + to_string(i) + " output.csv";

        // Process the orders and write to corresponding output file
        processOrders(inputFile, outputFile);
    }

    // Uncomment the below line to process custom input file
    // processOrders("input.csv", "output.csv");

    return 0;
}

