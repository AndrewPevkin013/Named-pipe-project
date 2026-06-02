#include <boost/interprocess/ipc/message_queue.hpp>

#include <iostream>
#include <string>

namespace bip = boost::interprocess;

int main() {
    const char* queue_name = "boost_limit_test_queue";

    constexpr std::size_t max_messages = 16;
    constexpr std::size_t max_message_size = 1024 * 1024;
    constexpr std::size_t too_large_message_size = max_message_size + 1;

    bip::message_queue::remove(queue_name);

    try {
        bip::message_queue mq(
            bip::create_only,
            queue_name,
            max_messages,
            max_message_size
        );

        std::string valid_message(max_message_size, 'A');
        std::string too_large_message(too_large_message_size, 'B');

        bool valid_sent = mq.try_send(
            valid_message.data(),
            valid_message.size(),
            0
        );

        bool too_large_sent = false;
        bool exception_caught = false;
        std::string exception_text;

        try {
            too_large_sent = mq.try_send(
                too_large_message.data(),
                too_large_message.size(),
                0
            );
        } catch (const bip::interprocess_exception& ex) {
            exception_caught = true;
            exception_text = ex.what();
        }

        std::cout << "Boost message_queue size limit test\n";
        std::cout << "Queue max messages: " << max_messages << "\n";
        std::cout << "Queue max message size(KB): "
                  << max_message_size / 1024 << "\n";
        std::cout << "Valid message size(KB): "
                  << valid_message.size() / 1024 << "\n";
        std::cout << "Too large message size(bytes): "
                  << too_large_message.size() << "\n";
        std::cout << "Valid message sent: "
                  << (valid_sent ? "yes" : "no") << "\n";
        std::cout << "Too large message sent: "
                  << (too_large_sent ? "yes" : "no") << "\n";
        std::cout << "Exception caught: "
                  << (exception_caught ? "yes" : "no") << "\n";

        if (exception_caught) {
            std::cout << "Exception: " << exception_text << "\n";
        }

        std::cout << "Conclusion: message_queue cannot send a message "
                     "larger than max_msg_size without manual fragmentation.\n";

        bip::message_queue::remove(queue_name);

#ifdef _WIN32
        system("pause");
#endif

        return 0;
    } catch (const bip::interprocess_exception& ex) {
        std::cerr << "Boost exception: " << ex.what() << "\n";
        bip::message_queue::remove(queue_name);

#ifdef _WIN32
        system("pause");
#endif

        return 1;
    }
}