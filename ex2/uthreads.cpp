//
// Created by noam.nezer on 6/4/24.
//
#include "uthreads.h"
#include <csetjmp>
#include <iostream>
#include <sys/time.h>
#include <list>
#include <queue>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <csignal>


#define INIT 0
#define JB_SP 6
#define JB_PC 7

typedef unsigned long address_t;


sigset_t signal_set;
enum STATE{
    READY,RUNNING, BLOCKED, SLEEPING
};

typedef struct Thread{
    STATE st;
    char stack[STACK_SIZE];
    sigjmp_buf env;
    int tid;
    int quantum_counter ;
    int sleep_time;
}thread_;

thread_*  pThread;

struct itimerval* timer;


void free_resources();

std::priority_queue<int, std::vector<int>, std::greater<int>> available_tid;
int max_tid ;

std::list<thread_ *> ready_list;

std::unordered_map<int,thread_*> all_threads;

std::unordered_map<int,thread_*> blocked_threads;

static std::unordered_map<int,thread_ *> sleeping_threads;

int running_tid;

static int total_quantums = 0;


///private
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

void mask_timer(){
    sigprocmask(SIG_BLOCK,&signal_set,NULL);
}
void unmask_timer(){
    sigprocmask(SIG_UNBLOCK,&signal_set,NULL);
}
int  set_timer(int quantum_usecs ){
    timer = (itimerval*)malloc(sizeof (itimerval));
    if( !timer){
        std::cerr << "system error: in set timer func \n" << std::endl;
    }
    timer->it_interval.tv_sec = quantum_usecs / 1000000;
    timer->it_interval.tv_usec=  quantum_usecs % 1000000;
    timer->it_value.tv_usec= quantum_usecs % 1000000;
    timer->it_value.tv_sec=  quantum_usecs / 1000000;

    if (setitimer(ITIMER_VIRTUAL, timer, nullptr) == -1) {

        free(pThread);
        free(timer);
        return 0;
    }
    return 1;

}

void timer_cycle(){
    ++total_quantums;
    std::unordered_map<int,thread_*> still_sleeping;

    for(auto &pair: sleeping_threads){
        //check for ==
        if(pair.second->sleep_time <= total_quantums) {
            pair.second->sleep_time = -1;
            if(pair.second->st == SLEEPING){
                pair.second->st = READY;
                ready_list.push_back(pair.second);
            }
        }
        else{
            still_sleeping[pair.first]=pair.second;
        }

    }
    sleeping_threads = still_sleeping;
    if (setitimer(ITIMER_VIRTUAL, timer, nullptr) == -1) {
        return ;
    }

}
int push_thread(){
    thread_  * next_thread = ready_list.front();
    ready_list.pop_front();
    next_thread->st = RUNNING;
    next_thread->quantum_counter++;


    running_tid = next_thread->tid;
    unmask_timer();
    siglongjmp(all_threads[running_tid]->env,1);

}

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */

void extract_thread(int current_tid){
    mask_timer();
    timer_cycle();
    int ret_val = sigsetjmp(all_threads[running_tid]->env,1);
    if(ret_val == 1){
        return;
    }
    thread_* thread = all_threads[running_tid];
    thread->st=READY;
    ready_list.push_back(thread);
    push_thread();
}
///-------------
void free_resources() {
    for (auto pair : all_threads){
        free(all_threads[pair.first]);
    }
    all_threads.clear();
    ready_list.clear();
    blocked_threads.clear();
    sleeping_threads.clear();

    pThread = nullptr;
    unmask_timer();
    exit(0);
}



//terminate one thread
void uthread_one_terminate(int tid){
    thread_* copy = all_threads[tid];
    available_tid.push(tid);
    all_threads.erase(tid);

    if(all_threads.size() == 0){
        unmask_timer();
        free_resources();
    }
    if(copy->st == RUNNING){
        timer_cycle();
        push_thread();
    }
    else if (copy->st == READY){
        ready_list.remove(copy);
    }
    else if (copy->st == BLOCKED){
        blocked_threads.erase(tid);
    }
    else { // sleeping
        sleeping_threads.erase(tid);
    }
    free(copy);
    unmask_timer();
}
/**
 *
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    max_tid =1;
    total_quantums=0;
    if(quantum_usecs <= 0){
        std::cerr << "thread library error: in uthread_init the quantum is<=0\n" << std::endl;
        return -1;
    }

    pThread =(thread_ *) malloc(sizeof(thread_));
    if(!pThread){
        std::cerr << "system error: in uthread_init the Main didnt init well\n" << std::endl;
        return -1;
    }
    ++total_quantums;
    pThread->st = RUNNING;
    pThread->tid = INIT;
    pThread->quantum_counter=1;
    pThread->sleep_time =-1;
    address_t sp = (address_t) pThread->stack + STACK_SIZE - sizeof(address_t);
    (pThread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (pThread->env->__jmpbuf)[JB_PC] = translate_address(0);
    all_threads[0]=pThread;

    if(!set_timer(quantum_usecs)){
        std::cerr << "system error: in uthread_init the set_timer() didnt init well\n" << std::endl;
        return -1;
    }
    struct sigaction sa{};
    sa.sa_handler = extract_thread;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0){
        std::cerr << "system error: in uthread_init the sigaction() didnt init well\n" << std::endl;
        return -1;
    }
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGVTALRM);
    running_tid = 0;
    return 0;
}


/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
    mask_timer();

    if (!entry_point ){
        std::cerr << "thread library error: in uthread_spawn entery point \n" << std::endl;
        unmask_timer();
        return -1;
    }
    if ( (max_tid - available_tid.size()) == MAX_THREAD_NUM){
        //todo add error
        std::cerr << "thread library error: in uthread_spawn max tid is pass  \n" << std::endl;
        unmask_timer();
        return -1;
    }
    int tid;
    if(available_tid.empty()){
        tid = max_tid++;
    }
    else{
        tid = available_tid.top();
        available_tid.pop();
    }
    thread_* new_thread = (thread_*) malloc(sizeof(thread_));
    if(!new_thread) {
        std::cerr << "system error: in uthread_spawn in create new threads\n" << std::endl;
        unmask_timer();
        return -1;
    }
    //create new thread
    new_thread->tid = tid;
    new_thread->sleep_time = -1;
    new_thread->st = READY;
    new_thread->quantum_counter=0;

    address_t sp = (address_t) new_thread->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(new_thread->env, 1);
    (new_thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (new_thread->env->__jmpbuf)[JB_PC] = translate_address(pc);

    if(sigemptyset(&new_thread->env->__saved_mask) == -1){
        std::cerr << "system error: in uthread_spawn in create new threads\n" << std::endl;
        return -1;
    }

    //put in array
    ready_list.push_back(new_thread);
    all_threads[new_thread->tid] = new_thread;
    unmask_timer();
    return new_thread->tid;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/

int uthread_terminate(int tid){
    mask_timer();

    if (all_threads.find(tid) == all_threads.end() && tid != 0){
        std::cerr << "thread library error: non valid input \n" << std::endl;
        unmask_timer();
        return -1;
    }
    if (tid ==0){
        free_resources();
    }
    else {
        uthread_one_terminate(tid);
        unmask_timer();
        return 0;
    }
    return 0;

}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    mask_timer();
    if (all_threads.find(tid) == all_threads.end() || tid == 0){
        std::cerr << "thread library error: invalid tid\n" << std::endl;
        unmask_timer();
        return -1;
    }
    thread_* thread = all_threads[tid];
    STATE st = thread->st;
    thread->st = BLOCKED;
    blocked_threads[tid] = thread;
    if (st == READY){
        ready_list.remove(thread);
    }
    else if (st == RUNNING){
        int ret_val = sigsetjmp(thread->env,1);
        if (ret_val==1){
            unmask_timer();
            return 0;
        }
        //todo - ret_val
        timer_cycle();
        push_thread();
    }
    else if (st == SLEEPING){
        //nada
    }
    unmask_timer();
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On succ
 * , return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    mask_timer();
    if (all_threads.find(tid) == all_threads.end() && tid != 0){
        std::cerr << "thread library error: in resume id \n" << std::endl;
        unmask_timer();
        return -1;
    }
    thread_* thread = all_threads[tid];
    STATE st = thread->st;
    if(st == BLOCKED){
        blocked_threads.erase(tid);
        if(thread->sleep_time == -1){//not sleeping
            ready_list.push_back(thread);
            thread->st = READY;
        }
        else{
            thread->st = SLEEPING;
        }
    }
    unmask_timer();
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
    mask_timer();
    if (running_tid == 0 || num_quantums <= 0){
        std::cerr << "thread library error: in sleep id \n" << std::endl;
        unmask_timer();
        return -1;
    }
    thread_* thread = all_threads[running_tid];
    thread->sleep_time = total_quantums + num_quantums + 1;
    thread->st = SLEEPING;
    sleeping_threads[running_tid] = thread;
    int ret_val = sigsetjmp(thread->env,1);
    if(ret_val == 1){
        unmask_timer();
        return 0;
    }
    timer_cycle();
    push_thread();
    unmask_timer();
    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    return running_tid;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    mask_timer();

    if (all_threads.find(tid) == all_threads.end() && tid != 0){
        std::cerr << "thread library error: invalid tid\n" << std::endl;
        unmask_timer();
        return -1;
    }
    thread_* thread = all_threads[tid];
    unmask_timer();
    return thread->quantum_counter;
}