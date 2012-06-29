#include <process/process.hpp>
#include <process/dispatch.hpp>

#include "utility.hpp"

typedef std::vector<uint64_t> factors;
constexpr uint64_t s_task_n = uint64_t(86028157)*329545133;
constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;

void check_factors(const factors& vec)
{
    assert(vec.size() == 2);
    assert(vec[0] == s_factor1);
    assert(vec[1] == s_factor2);
}

class supervisor : public process::Process<supervisor>
{
public:
    supervisor(int num_msgs) 
      : left_(num_msgs)
    {
    }

    void subtract()
    {
        if (--left_ == 0)
            process::terminate(self());
    }

    void check(const factors& vec)
    {
        check_factors(vec);
        if (--left_ == 0)
            process::terminate(self());
    }

private:
    int left_;
};

class chain_link : public process::Process<chain_link>
{
public:
    chain_link()
    {
    }

    chain_link(process::PID<chain_link> pid)
      : next_(std::move(pid))
    {
    }

    virtual ~chain_link() { }

    virtual void token(int v)
    {
        assert(next_);
        process::dispatch(next_, &chain_link::token, v);
        if (v == 0)
            process::terminate(self());
    }

private:
    process::PID<chain_link> next_;
};

class worker : public process::Process<worker>
{
public:
    worker(process::PID<supervisor> collector) 
      : collector_(std::move(collector))
    {
    }

    void calc(uint64_t what)
    {
        process::dispatch(collector_, &supervisor::check, factorize(what));
    }

    void done()
    {
        process::terminate(self());
    }

private:
    process::PID<supervisor> collector_;
};

class chain_master : public chain_link
{
public:
    chain_master(process::PID<supervisor> collector, int rs, int itv, int n)
      : chain_link()
      , ring_size_(rs)
      , initial_value_(itv)
      , repetitions_(n)
      , iteration_(0)
      , collector_(collector)
    {
    }

    void new_ring(int ring_size, int initial_token_value)
    {
        process::dispatch(worker_, &worker::calc, s_task_n);
        next_ = self();
        for (int i = 1; i < ring_size; ++i)
            next_ = process::spawn(new chain_link(next_), true);

        process::dispatch(next_, &chain_link::token, initial_token_value);
    }

    void init()
    {
        worker_ = process::spawn(new worker(collector_), true);
        new_ring(ring_size_, initial_value_);
    }

    virtual void token(int t)
    {
        if (t == 0)
        {
            if (++iteration_ < repetitions_)
            {
                new_ring(ring_size_, initial_value_);
            }
            else
            {
                dispatch(worker_, &worker::done);
                dispatch(collector_, &supervisor::subtract);
                process::terminate(self());
            }
        }
        else
        {
            process::dispatch(next_, &chain_link::token, t - 1);
        }
    }

private:
    const int ring_size_;
    const int initial_value_;
    const int repetitions_;
    int iteration_;
    process::PID<supervisor> collector_;
    process::PID<chain_link> next_;
    process::PID<worker> worker_;
};

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cout << "usage " << argv[0] << ": " <<
            "(num rings) (ring size) (initial token value) (repetitions)"
            << std::endl;

        return 1;
    }

    auto iter = argv;
    ++iter; // argv[0] (app name)
    int num_rings = rd<int>(*iter++);
    int ring_size = rd<int>(*iter++);
    int initial_token_value = rd<int>(*iter++);
    int repetitions = rd<int>(*iter++);
    int num_msgs = num_rings + (num_rings * repetitions);

    auto mc = process::spawn(new supervisor(num_msgs), true);
    std::vector<process::PID<chain_master>> masters;
    for (int i = 0; i < num_rings; ++i)
    {
        auto master = process::spawn(
            new chain_master(mc, ring_size, initial_token_value, repetitions),
            true);

        process::dispatch(master, &chain_master::init);
        masters.push_back(master);
    }

    for (auto& m : masters)
        process::wait(m);

    return 0;
}
