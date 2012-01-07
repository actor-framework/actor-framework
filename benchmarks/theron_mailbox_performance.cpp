#define THERON_USE_BOOST_THREADS 1

#include <Theron/Framework.h> 
#include <Theron/Actor.h> 

#include <iostream>
#include <thread>
#include <cstdint>

using std::cerr;
using std::cout;
using std::endl;
using std::int64_t;

// A trivial actor that does nothing. 
struct receiver : Theron::Actor 
{

	int64_t m_max;
	int64_t m_num;
	Theron::Address m_waiter;

    typedef struct { int64_t arg1; Theron::Address arg2; } Parameters;

	void handler(int64_t const&, Theron::Address)
	{
		if (++m_num == m_max)
			Send(m_max, m_waiter);
	}

	//receiver(int64_t max, Theron::Address waiter) : m_max(max), m_num(0), m_waiter(waiter)
    receiver(Parameters const& p) : m_max(p.arg1), m_num(0), m_waiter(p.arg2)
	{
		RegisterHandler(this, &receiver::handler);
	}

}; 

void sender(Theron::ActorRef ref, int64_t num)
{
	int64_t msg;
	for (int64_t i = 0; i < num; ++i)
	{
		ref.Push(msg, ref.GetAddress());
	}
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		cout << "usage (num_threads) (num_messages)" << endl;
		return 1;
	}
	char* endptr = nullptr;
        int64_t num_sender = static_cast<int64_t>(strtol(argv[1], &endptr, 10));
        if (endptr == nullptr || *endptr != '\0')
        {
            cerr << "\"" << argv[1] << "\" is not an integer" << endl;
            return 1;
        }
        int64_t num_msgs = static_cast<int64_t>(strtol(argv[2], &endptr, 10));
        if (endptr == nullptr || *endptr != '\0')
        {
            cerr << "\"" << argv[2] << "\" is not an integer" << endl;
            return 1;
        }
	Theron::Receiver r;
	Theron::Framework framework; 
	Theron::ActorRef aref(framework.CreateActor<receiver>(receiver::Parameters{num_sender*num_msgs, r.GetAddress()})); 
	for (int64_t i = 0; i < num_sender; ++i)
        {
            std::thread(sender, aref, num_msgs).detach();
        }
	r.Wait();
	return 0; 
} 
