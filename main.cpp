#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream> 
#include <atomic>
#include <array>
#include <memory>
#include <map>
#include <type_traits>
#include <stack>
#include <new>
#include <cstddef>
#include <thread>

constexpr std::uint32_t kSlote1000MsCount = 150;
constexpr std::uint32_t kSlote100MsCount = 200;
constexpr std::uint32_t kSlote50MsCount = 200;
constexpr std::uint32_t kAreanaSize = 10;
using timeStampType = std::uint64_t;

struct BigData{
    std::array<std::uint32_t,kSlote1000MsCount> slot_1000Ms;
    std::array<std::uint32_t,kSlote100MsCount> slot_100Ms;
    std::array<std::uint32_t,kSlote50MsCount> slot_50Ms;
    BigData(){
        std::fill(slot_1000Ms.begin(),slot_1000Ms.end(),0);
        std::fill(slot_100Ms.begin(),slot_100Ms.end(),0);
        std::fill(slot_50Ms.begin(),slot_50Ms.end(),0);
    }
};


template <typename T, std::size_t ArenaSize = kAreanaSize>
class ArenaAllocator {
public:
    typedef T value_type;

    ArenaAllocator() throw() {}

    template <typename U>
    ArenaAllocator(const ArenaAllocator<U, ArenaSize>&) throw() {}

    T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_alloc();

        if (!freeList_.empty()) {
            T* ptr = freeList_.top();
            freeList_.pop();
            std::cout << "Allocate [Arena Reuse]: " << ptr << "\n";
            return ptr;
        }

        if (arenaIndex_ < ArenaSize) {
            T* ptr = reinterpret_cast<T*>(&arena_[arenaIndex_++]);
            std::cout << "Allocate [Arena New]: " << ptr << "\n";
            return ptr;
        }

        T* ptr = static_cast<T*>(::operator new(sizeof(T)));
        std::cout << "Allocate [Dynamic]: " << ptr << "\n";
        if(!ptr){
            throw std::bad_alloc();    
        }
        
        return ptr;
    }

    void deallocate(T* p, std::size_t) throw() {
        if (is_in_arena(p)) {
            freeList_.push(p);
            std::cout << "Deallocate [Arena]: " << p << "\n";
        } else {
            std::cout << "Deallocate [Dynamic]: " << p << "\n";
            ::operator delete(p);
        }
    }

    template <typename U>
    struct rebind {
        typedef ArenaAllocator<U, ArenaSize> other;
    };

    template <typename U, std::size_t N>
    friend bool operator==(const ArenaAllocator&, const ArenaAllocator<U, N>&) { return true; }

    template <typename U, std::size_t N>
    friend bool operator!=(const ArenaAllocator&, const ArenaAllocator<U, N>&) { return false; }

private:
    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type StorageType;

    static StorageType arena_[ArenaSize];
    static std::size_t arenaIndex_;
    static std::stack<T*> freeList_;

    bool is_in_arena(void* p) const {
        uintptr_t begin = reinterpret_cast<uintptr_t>(&arena_[0]);
        uintptr_t end = reinterpret_cast<uintptr_t>(&arena_[ArenaSize]);
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return addr >= begin && addr < end;
    }
};

template <typename T, std::size_t ArenaSize>
typename ArenaAllocator<T, ArenaSize>::StorageType ArenaAllocator<T, ArenaSize>::arena_[ArenaSize];

template <typename T, std::size_t ArenaSize>
std::size_t ArenaAllocator<T, ArenaSize>::arenaIndex_ = 0;

template <typename T, std::size_t ArenaSize>
std::stack<T*> ArenaAllocator<T, ArenaSize>::freeList_;



int main(){
    std::cout << "[Custom Allocator] Start\n";
    std::map<timeStampType,BigData, std::less<timeStampType>,
        ArenaAllocator<std::pair<const timeStampType, BigData>>> big_data_buffer;

    std::atomic<bool> stop{false};    
    big_data_buffer.emplace(0,BigData());
    big_data_buffer.emplace(1,BigData());
    big_data_buffer.emplace(2,BigData());
    big_data_buffer.emplace(3,BigData());
    big_data_buffer.emplace(4,BigData());

    std::thread inserter_thread ([&](){
        while(1){
            static std::uint32_t counter{5};
            big_data_buffer.emplace(counter,BigData());
            std::cout << "Emplaced BigData Struct ["<<counter<<"] at Address: "<< &big_data_buffer.end()->first<<std::endl; 
            counter++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout<<"-----------------------------------------------------------------------------------\n";
            if(stop) return;
        }
    });

    std::thread consume_thread ([&](){  
        while(1){
            if(!big_data_buffer.empty()){

                std::cout << "Remove BigData Struct ["<<big_data_buffer.begin()->first<<"] at Address: "<< &big_data_buffer.begin()->first<<std::endl; 
                big_data_buffer.erase(big_data_buffer.begin());
                if(big_data_buffer.size() > 3)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }else{
                std::cout << "Empty  "<<std::endl; 
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout<<"-----------------------------------------------------------------------------------\n";
            if(stop) return;
        }      
    });


    std::this_thread::sleep_for(std::chrono::seconds(300));
    stop = true;

    consume_thread.join();
    inserter_thread.join();

    return 0;
}


