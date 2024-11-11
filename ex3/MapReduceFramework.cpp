#include <cstdlib>
#include <unordered_map>
#include <pthread.h>
#include "MapReduceFramework.h"
#include "MapReduceClient.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <map>
#include <algorithm>

#include "Barrier.h"

// Forward declarations
struct thread_pair;
struct thread_context;
class Job;

static void* execute_thread(void* context);
bool comparePairs(const IntermediatePair& a, const IntermediatePair& b);
bool K2_comparator(const K2* a, const K2* b);

static void shuffle(Job *job,int id);
static void reduce(Job *job);
static void map_(thread_context *cur, Job *job);
static void map_(thread_context *cur, Job *job);

typedef std::unordered_map<int, thread_pair*> ThreadContextMap;
typedef std::vector<IntermediatePair> v2_vector;
typedef std::map<K2*, v2_vector *, bool(*)(const K2*, const K2*)> ShuffleMap;


// Definition of context and threadID structs
struct thread_pair {
    thread_context* context;
    pthread_t* thread;
};
struct thread_context{
    Job* job;
    IntermediateVec* intermediateVec;
    int id;

};

void print_error(const std::string &message)  {
    std::cout << "system error: "+ message +"\n" << std::endl;
    exit(1);
}



class Job {
public:

    const MapReduceClient& client;
    std::shared_ptr<InputVec> inputVec;
    OutputVec &outputVec;
    int multiThreadLevel;
    std::atomic<bool> atomic_mutex;
    std::atomic<bool> is_waited;
    ThreadContextMap thread_context_map;
    stage_t stage;
    pthread_mutex_t mutex;
    Barrier barrier;
    ShuffleMap shuffle_map;
    int shuffleCount;
    float sumOfVector;
    float current_input_vec_index;
    float inputvec_size;
    float total_reduce_phase,current_reduce_phase;


    Job(const MapReduceClient& client,
        std::shared_ptr<InputVec> inputVec, OutputVec& outputVec,
        int multiThreadLevel)
            : client(client), inputVec(inputVec),outputVec(outputVec),
              multiThreadLevel(multiThreadLevel), atomic_mutex(false)
              ,is_waited(false),stage(UNDEFINED_STAGE), barrier(multiThreadLevel),
              shuffleCount(0),current_input_vec_index(0),total_reduce_phase(0),current_reduce_phase(0)
    {
        inputvec_size =inputVec->size();


        pthread_mutex_init(&mutex, nullptr);
        atomic_mutex.store(false);
        is_waited.store(false);
        for (int i = 0; i < multiThreadLevel; ++i) {
            pthread_t* thread = new pthread_t();
            thread_context* ctx = new thread_context();
            ctx->job = this;
            ctx->id=i;

            ctx->intermediateVec = new IntermediateVec();
            thread_pair* thread_and_ctx = new thread_pair();
            thread_and_ctx->thread = thread;
            thread_and_ctx->context = ctx;

            thread_context_map.emplace(i, thread_and_ctx);
        }
        shuffle_map = ShuffleMap(K2_comparator);
        //create the thread
        for (int i = 0; i<multiThreadLevel; ++i){

            if(pthread_create(thread_context_map[i]->thread, NULL, execute_thread, thread_context_map[i]->context) != 0){
                print_error("error in creating thread");
            }

        }
    }
    void lock_mutex_map(){
        if(pthread_mutex_lock(&mutex) != 0){
            print_error("error locking mutex map");
        }
    }

    void unlock_mutex_map(){
        if(pthread_mutex_unlock(&mutex) != 0){
            print_error("error unlocking mutex map");
        }
    }



    ~Job() {
        for (auto& pair : thread_context_map) {

            delete pair.second->context->intermediateVec;
            delete pair.second->context;
            delete pair.second->thread;
            delete pair.second;
        }


        if( pthread_mutex_destroy(&this->mutex) != 0){
//            delete this;
            print_error("error destroying mutex");
        };
    }
};
JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
    // Create a shared pointer to inputVec to pass to Job
    auto sharedInputVec = std::make_shared<InputVec>(std::move(inputVec));

    // Create a new Job instance and return its handle
    return new Job(client, sharedInputVec, outputVec, multiThreadLevel);
}

void waitForJob(JobHandle job) {
    Job * jobHandle = (Job *)job;

    if(!jobHandle->is_waited){
        jobHandle->is_waited.store(true);
        for(auto &thread :jobHandle->thread_context_map){
            if(pthread_join(*thread.second->thread, NULL)!= 0){
                print_error("error using pthread_join in waitForJob");
            }
        }
    }
}

void getJobState(JobHandle job, JobState* state) {
    Job * jobHandle = static_cast<Job *>(job);
    jobHandle->lock_mutex_map();
    state->stage = jobHandle->stage;
    switch(state->stage) {

        case UNDEFINED_STAGE:
            state->percentage = 0;
            break;
        case MAP_STAGE:
            state->percentage = 100 *(jobHandle->current_input_vec_index/jobHandle->inputvec_size);
            break;
        case SHUFFLE_STAGE:
            state->percentage = 100*((float)jobHandle->shuffleCount/jobHandle->sumOfVector);
            break;
        case REDUCE_STAGE:
            state->percentage = 100*(jobHandle->current_reduce_phase/jobHandle->total_reduce_phase);
            break;
        default:
            break;
    }
    jobHandle->unlock_mutex_map();

}
void closeJobHandle(JobHandle job) {
    Job * jobHandle = static_cast<Job *>(job);
    waitForJob(jobHandle);
    delete jobHandle;
}

void emit2(K2* key, V2* value, void* context) {
    thread_context * threadAndContext =(thread_context *)context;

    threadAndContext->intermediateVec->push_back( IntermediatePair (key, value));
}

void emit3(K3* key, V3* value, void* context) {
    // Placeholder implementation for emitting final key-value pairs
    Job* job = (Job*) context;
    job->outputVec.push_back(OutputPair (key, value));
//    job->outputVec.emplace_back(key, value);
}
void printVector( IntermediateVec *vec) {
    for ( auto& pair : *vec) {
        std::cout << "(" << pair.first << ", " << pair.second << ") ";
    }
    std::cout << std::endl;
}
static void* execute_thread(void* context) {
    thread_context*  cur =(thread_context * ) context;
    Job* job = cur->job;
    //map
    map_(cur, job);

//    printVector(cur->intermediateVec);
    //sort
    std::sort(cur->intermediateVec->begin(), cur->intermediateVec->end(), comparePairs);
    //wait for all thread to finish sorting
    job->barrier.barrier();
    //shuffle
    shuffle(job, cur->id);
    //wait for all thread to finish sorting
    job->barrier.barrier();
    //reduce
    reduce(job);
    //wait for all thread to finish reduce

    job->barrier.barrier();
    return 0;
}

static void map_(thread_context *cur, Job *job) {
    job->stage = MAP_STAGE;
    job->lock_mutex_map();
    while(!job->inputVec->empty()){

        InputPair inputPair = job->inputVec->back();

        job->inputVec->erase(job->inputVec->cend());
        job->current_input_vec_index++;
        job->unlock_mutex_map();
        job->client.map(inputPair.first,inputPair.second,cur);
        job->lock_mutex_map();
    }
    job->sumOfVector+=cur->intermediateVec->size();

    job->unlock_mutex_map();
}

static void reduce(Job *job){
    job->stage=REDUCE_STAGE;
    job->lock_mutex_map();
    while(!job->shuffle_map.empty()){
        auto entry = job->shuffle_map.begin();
        v2_vector* vec = entry->second;
        job->shuffle_map.erase(entry->first);
        job->current_reduce_phase++;
        job->client.reduce(vec,job);
        delete vec;
        job->unlock_mutex_map();
        job->lock_mutex_map();
    }
    job->unlock_mutex_map();

}
static void shuffle(Job *job, int id) {
    job->stage = SHUFFLE_STAGE;
    if(id != 0){
        return;
    }
    for (auto& entry: job->thread_context_map){
        for(auto& pair:*entry.second->context->intermediateVec ){
            if(job->shuffle_map.find(pair.first) == job->shuffle_map.end()){
                v2_vector* vector = new v2_vector();
                vector->push_back(pair);
                job->shuffle_map.emplace(pair.first,vector);
            }
            else{
                job->shuffle_map[pair.first]->push_back(pair);
            }
            job->shuffleCount++;

        }
    }
    job->total_reduce_phase=job->shuffle_map.size();
}



bool K2_comparator(const K2* a, const K2* b) {

    return *b < *a;
}

bool comparePairs(const IntermediatePair& a, const IntermediatePair& b) {

    return K2_comparator(a.first, b.first);
}
