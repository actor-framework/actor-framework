#include <thread>
#include <vector>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <stdexcept>

//#include <boost/thread.hpp>
#include <boost/progress.hpp>
#include <boost/lexical_cast.hpp>

#include "sutter_list.hpp"
#include "cached_stack.hpp"
#include "lockfree_list.hpp"
#include "blocking_sutter_list.hpp"
#include "intrusive_sutter_list.hpp"
#include "blocking_cached_stack.hpp"
#include "blocking_cached_stack2.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace {

//const size_t num_messages = 1000000;
//const size_t num_producers = 10;
//const size_t num_messages_per_producer = num_messages / num_producers;

} // namespace <anonymous>

template<typename Queue, typename Allocator>
void producer(Queue& q, Allocator& a, size_t begin, size_t end) {
    for ( ; begin != end; ++begin) {
        q.push(a(begin));
    }
}

template<typename Queue, typename Processor>
void consumer(Queue& q, Processor& p, size_t num_messages) {
    // vector<bool> scales better (in memory) than bool[num_messages]
//	std::vector<bool> received(num_messages);
//	for (size_t i = 0; i < num_messages; ++i) received[i] = false;
    for (size_t i = 0; i < num_messages; ++i) {
        size_t value;
        p(q.pop(), value);
        /*
        if (value >= num_messages) {
            throw std::runtime_error("value out of bounds");
        }
        else if (received[value]) {
            std::ostringstream oss;
            oss << "ERROR: received element nr. " << value << " two times";
            throw std::runtime_error(oss.str());
        }
        received[value] = true;
        */
    }
    // done
}

void usage() {
    cout << "usage:" << endl
         << "queue_test [messages] [producer threads] "
            "[list impl.] {format string}" << endl
         << "    available implementations:" << endl
         << "    - sutter_list" << endl
         << "    - intrusive_sutter_list" << endl
         << "    - blocking_sutter_list" << endl
         << "    - cached_stack" << endl
         << "    - blocking_cached_stack" << endl
         << "    - blocking_cached_stack2" << endl
         << "    - lockfree_list" << endl
         << endl
         << "    possible format string variables: " << endl
         << "    - MESSAGES" << endl
         << "    - MSG_IN_MILLION" << endl
         << "    - PRODUCERS" << endl
         << "    - TIME" << endl
         << endl
         << "example: ./queue_test 10000 10 cached_stack \"MESSAGES TIME\""
         << endl;
}

template<typename Queue, typename Allocator, typename Processor>
double run_test(size_t num_messages, size_t num_producers,
                Allocator element_allocator, Processor element_processor) {
    size_t num_messages_per_producer = num_messages / num_producers;
    // measurement
    boost::timer t0;
    // locals
    Queue list;
    std::vector<std::thread*> producer_threads(num_producers);
    for (size_t i = 0; i < num_producers; ++i) {
        producer_threads[i] = new std::thread(producer<Queue, Allocator>,
                                              std::ref(list),
                                              std::ref(element_allocator),
                                              i * num_messages_per_producer, (i+1) * num_messages_per_producer);
    }
    // run consumer in main thread
    consumer(list, element_processor, num_messages);
    // print result
    return t0.elapsed();

}

struct cs_element {
    size_t value;
    std::atomic<cs_element*> next;
    cs_element(size_t val = 0) : value(val), next(0) { }
};

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        usage();
        return -1;
    }
    size_t num_messages = boost::lexical_cast<size_t>(argv[1]);
    size_t num_producers = boost::lexical_cast<size_t>(argv[2]);
    if (num_messages == 0 || num_producers == 0) {
        cerr << "invalid arguments" << endl;
        return -2;
    }
    if ((num_messages % num_producers) != 0) {
        cerr << "(num_messages % num_producers) != 0" << endl;
        return -3;
    }
    std::string format_string;
    if (argc == 5) {
        format_string = argv[4];
    }
    else {
        format_string = "$MESSAGES $TIME";
    }
    std::string list_name = argv[3];
    double elapsed_time;
    if (list_name == "sutter_list") {
        elapsed_time = run_test<sutter_list<size_t>>(
                num_messages,
                num_producers,
                [] (size_t value) -> size_t* {
                    return new size_t(value);
                },
                [] (size_t* value, size_t& storage) {
                    storage = *value;
                    delete value;
                }
            );
    }
    else if (list_name == "intrusive_sutter_list") {
        typedef intrusive_sutter_list<size_t> isl;
        elapsed_time = run_test<isl>(
                num_messages,
                num_producers,
                [] (size_t value) -> isl::node* {
                    return new isl::node(value);
                },
                [] (const size_t& value, size_t& storage) {
                    storage = value;
                }
            );
    }

    else if (list_name == "lockfree_list") {
        typedef lockfree_list<size_t> isl;
        elapsed_time = run_test<isl>(
                num_messages,
                num_producers,
                [] (size_t value) -> isl::node* {
                    return new isl::node(value);
                },
                [] (const size_t& value, size_t& storage) {
                    storage = value;
                }
            );
    }
    else if (list_name == "blocking_sutter_list") {
        elapsed_time = run_test<blocking_sutter_list<size_t>>(
                num_messages,
                num_producers,
                [] (size_t value) -> size_t* {
                    return new size_t(value);
                },
                [] (size_t* value, size_t& storage) {
                    storage = *value;
                    delete value;
                }
            );
    }
    else if (list_name == "cached_stack") {
        elapsed_time = run_test<cached_stack<cs_element>>(
                num_messages,
                num_producers,
                [] (size_t value) -> cs_element* {
                    return new cs_element(value);
                },
                [] (cs_element* e, size_t& storage) {
                    storage = e->value;
                    delete e;
                }
            );
    }
    else if (list_name == "blocking_cached_stack") {
        elapsed_time = run_test<blocking_cached_stack<cs_element>>(
                num_messages,
                num_producers,
                [] (size_t value) -> cs_element* {
                    return new cs_element(value);
                },
                [] (cs_element* e, size_t& storage) {
                    storage = e->value;
                    delete e;
                }
            );
    }
    else if (list_name == "blocking_cached_stack2") {
        elapsed_time = run_test<blocking_cached_stack2<cs_element>>(
                num_messages,
                num_producers,
                [] (size_t value) -> cs_element* {
                    return new cs_element(value);
                },
                [] (cs_element* e, size_t& storage) {
                    storage = e->value;
                    delete e;
                }
            );
    }
    else {
        cerr << "unknown list" << endl;
        usage();
        return -4;
    }
    // build output message
    std::vector<std::pair<std::string, std::string>> replacements = { { "MESSAGES", argv[1] }, { "PRODUCERS", argv[2] }, { "MSG_IN_MILLION", boost::lexical_cast<std::string>(num_messages / 1000000.0) }, { "TIME", boost::lexical_cast<std::string>(elapsed_time) }
    };
    for (auto i = replacements.begin(); i != replacements.end(); ++i) {
        const std::string& needle = i->first;
        const std::string& value = i->second;
        std::string::size_type pos = format_string.find(needle);
        if (pos != std::string::npos) {
            format_string.replace(pos, pos + needle.size(), value);
        }
    }
    cout << format_string << endl;
    // done
    return 0;
}
