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
    }

    // Function to validate order fields
    void validateOrder(string* clientOrderID, string* instrument, string* side, string* qty, string* price)
    {
        if (!((*clientOrderID).length() && (*instrument).length() && (*side).length() && (*price).length() && (*qty).length()))
        {
            Status = 1;
            Reason = "Invalid Fields";
            return;
        }

        if (find(begin(Instruments), end(Instruments), *instrument) == end(Instruments))
        {
            Status = 1;
            Reason = "Invalid Instrument";
            return;
        }

        int s = stoi(*side);
        if (s != 1 && s != 2)
        {
            Status = 1;
            Reason = "Invalid Side";
            return;
        }

        double p = stod(*price);
        if (!(p > 0.0))
        {
            Status = 1;
            Reason = "Invalid Price";
            return;
        }

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

    bool isValid()
    {
        return Status == 0;
    }

    // Execute the order and write details to output file
    void execute(ofstream& output, int execQty, double execPrice)
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
               << Side << "," << statusStr << "," << execQty << ","  << execPrice
               << "," << Reason << "," << put_time(lt, "%Y.%m.%d-%H.%M.%S")
               << "." << setfill('0') << setw(3) << msCount % 1000 << "\n";
    }

    void execute(ofstream& output)
    {
        execute(output, RemainingQty, Price);
    }
};

int Order::CurrentOrderID = 1;

// Insert new order into the appropriate order book
void addOrderToBook(vector<Order>& orderBook, Order newOrder, int side)
{
    auto it = begin(orderBook);
    while (it != end(orderBook))
    {
        if ((it->Price < newOrder.Price && side == 1) || (it->Price > newOrder.Price && side == 2))
        {
            break;
        }
        ++it;
    }
    orderBook.insert(it, newOrder);
}

void processOrders(string inputFile, string outputFile)
{
    vector<vector<Order>> OrderBook_Rose(2), OrderBook_Lavender(2), OrderBook_Lotus(2), OrderBook_Tulip(2), OrderBook_Orchid(2);
    vector<vector<Order>> allBooks[5] = {OrderBook_Rose, OrderBook_Lavender, OrderBook_Lotus, OrderBook_Tulip, OrderBook_Orchid};
    string Instruments[5] = {"Rose", "Lavender", "Lotus", "Tulip", "Orchid"};

    ifstream input(inputFile);
    ofstream output(outputFile);
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
            vector<vector<Order>>& orderBook = allBooks[bookIndex];

            if (newOrder.Side == 1)  // Buy side
            {
                while (!orderBook[1].empty() && (orderBook[1][0].Price <= newOrder.Price))
                {
                    if (newOrder.RemainingQty == orderBook[1][0].RemainingQty)
                    {
                        newOrder.Status = 2;
                        orderBook[1][0].Status = 2;
                        newOrder.execute(output, newOrder.RemainingQty, orderBook[1][0].Price);
                        orderBook[1][0].execute(output, newOrder.RemainingQty, orderBook[1][0].Price);
                        newOrder.RemainingQty = 0;
                        orderBook[1][0].RemainingQty = 0;
                        orderBook[1].erase(begin(orderBook[1]));
                        break;
                    }
                    else if (newOrder.RemainingQty > orderBook[1][0].RemainingQty)
                    {
                        newOrder.Status = 3;
                        orderBook[1][0].Status = 2;
                        newOrder.execute(output, orderBook[1][0].RemainingQty, orderBook[1][0].Price);
                        orderBook[1][0].execute(output, orderBook[1][0].RemainingQty, orderBook[1][0].Price);
                        newOrder.RemainingQty -= orderBook[1][0].RemainingQty;
                        orderBook[1].erase(begin(orderBook[1]));
                    }
                    else
                    {
                        newOrder.Status = 2;
                        orderBook[1][0].Status = 3;
                        newOrder.execute(output, newOrder.RemainingQty, orderBook[1][0].Price);
                        orderBook[1][0].execute(output, newOrder.RemainingQty, orderBook[1][0].Price);
                        orderBook[1][0].RemainingQty -= newOrder.RemainingQty;
                        newOrder.RemainingQty = 0;
                        break;
                    }
                }

                if (newOrder.RemainingQty > 0)
                {
                    addOrderToBook(orderBook[0], newOrder, 1);
                }
            }
            else if (newOrder.Side == 2)  // Sell side
            {
                while (!orderBook[0].empty() && (orderBook[0][0].Price >= newOrder.Price))
                {
                    if (newOrder.RemainingQty == orderBook[0][0].RemainingQty)
                    {
                        newOrder.Status = 2;
                        orderBook[0][0].Status = 2;
                        newOrder.execute(output, newOrder.RemainingQty, orderBook[0][0].Price);
                        orderBook[0][0].execute(output, newOrder.RemainingQty, orderBook[0][0].Price);
                        newOrder.RemainingQty = 0;
                        orderBook[0][0].RemainingQty = 0;
                        orderBook[0].erase(begin(orderBook[0]));
                        break;
                    }
                    else if (newOrder.RemainingQty > orderBook[0][0].RemainingQty)
                    {
                        newOrder.Status = 3;
                        orderBook[0][0].Status = 2;
                        newOrder.execute(output, orderBook[0][0].RemainingQty, orderBook[0][0].Price);
                        orderBook[0][0].execute(output, orderBook[0][0].RemainingQty, orderBook[0][0].Price);
                        newOrder.RemainingQty -= orderBook[0][0].RemainingQty;
                        orderBook[0].erase(begin(orderBook[0]));
                    }
                    else
                    {
                        newOrder.Status = 2;
                        orderBook[0][0].Status = 3;
                        newOrder.execute(output, newOrder.RemainingQty, orderBook[0][0].Price);
                        orderBook[0][0].execute(output, newOrder.RemainingQty, orderBook[0][0].Price);
                        orderBook[0][0].RemainingQty -= newOrder.RemainingQty;
                        newOrder.RemainingQty = 0;
                        break;
                    }
                }

                if (newOrder.RemainingQty > 0)
                {
                    addOrderToBook(orderBook[1], newOrder, 2);
                }
            }
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
//    for (int i = 1; i <= 8; i++)
//    {
//        string inputFile = "inputs/Example " + to_string(i) + ".csv";
//        ifstream input(inputFile);
//        if (!input.is_open())
//        {
//            cerr << "Error: Unable to open input file: " << inputFile << endl;
//            continue;  // Skip to the next file
//        }
//
//        string outputFile = "./outputs/Example " + to_string(i) + " output.csv";
//        ofstream output(outputFile);
//        if (!output.is_open())
//        {
//            cerr << "Error: Unable to open output file: " << outputFile << endl;
//            continue;
//        }
//
//        processOrders(inputFile, outputFile);
//    }

    processOrders("ex1_input_buy_sell_all_NEW_5.csv", "output1.csv");
    processOrders("ex2_input_simple_FILL_5.csv", "output2.csv");
    processOrders("ex3_input_two_orderbooks_5.csv", "output3.csv");
    processOrders("ex4_input_Aggreesing_Market_SELL_Order_10.csv", "output4.csv");
    processOrders("ex5_input_Aggressing_Market_BUY_folllowedby_SELL_10.csv", "output5.csv");
    processOrders("ex6_input_Validations_5.csv", "output6.csv");
    processOrders("ex7_input_TimePriority_SortOrder_10.csv", "output7.csv");


    return 0;
}
