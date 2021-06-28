#include <iostream>

// boost
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include "client.hpp"

int main(int argc, const char* argv[]) {

    boost::asio::io_context ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(ioc.get_executor());

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        ioc.stop();
        std::cout << "main ioc exit" << std::endl;
    });

    std::cout << "main start." << std::endl;
    
    std::shared_ptr<Client> client = Client::Create(ioc);

    client->Start();

    ioc.run();

    client->Stop();
    std::cout << "main exit." << std::endl;

    return 0;
}