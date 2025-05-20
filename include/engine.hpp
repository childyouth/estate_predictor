#ifndef __ENGINE_HPP__
#define __ENGINE_HPP__

#include "generals.hpp"
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/fiber/all.hpp>
#include <boost/lockfree/queue.hpp>

#define WORKER_MAX_WORKLOAD 128

enum class api_type{
    RENT,
    TRADE,
    NUM_API_TYPE // api_type 수
};


struct api_msg{
    api_type api;
    ldcd_t lawd_cd;
    ym_t deal_ymd;
};

struct api_information{
    // base, endpoint, api_key
    std::vector<std::tuple<std::string, std::string, std::string>> api_addr_and_key;
    
};

struct worker_context{
    int id;

    // coroutine이 확인할 땐 race condition이 없음
    // 따라서 atomic 불필요

    u_int num_working_tasks;            // 현재 진행중인 task 수 (상한 :WORKER_MAX_WORKLOAD)
    u_int num_finished_tasks;           // worker가 수행완료한 task의 수
    u_int num_failed_tasks;              // 수행 실패한 task 수
    std::vector<api_msg*> error_tasks;  // 수행 실패한 task의 api_msg pointer

    std::ofstream savefile;
    
    std::atomic<bool> is_done;
};


class engine{
private:
    void push_task(api_msg* api_msg_ptr);
    bool pull_task(api_msg*& api_msg_ptr);
    void allocate_work();                   // lawd_cd 단위 work producer
    void allocate_task(ldcd_t lawd_cd);        // lawd_cd 의 deal_ymd 단위 task producer (msg producer)
    ym_t* generate_year_month_list();       // ini에서 시작~끝 ymd 가져와 list 생성
    ldcd_t* parse_lawd_cd_list();           // binary파일에서 lawd_cd 가져오기
    void parse_api_info();
    void parse_app_args();
    void init_worker_ctx(int thread_id);
    bool is_worker_busy(worker_context *worker_ctx);
    bool is_worker_busy(int thread_id);
    bool is_worker_working(worker_context *ctx);
    bool is_worker_working(int thread_id);
    boost::asio::awaitable<void> handle_api_msg(api_msg* msg, worker_context* worker_ctx);
    int worker(int thread_id);
    // void consumer();
    // void producer();

public:
    engine(std::string _ini_filename);
    // ~engine();
    void run();

private:
    ym_t* year_month_list;          // deal_ymd 전체 리스트
    int year_month_list_len;

    ldcd_t* lawd_cd_list;            // 법정동코드 리스트
    int lawd_cd_list_len;

    api_information api_info;

    boost::property_tree::ptree ini_ptree;                // ini 파일 ptree

    std::string ini_filename;       // ini 파일 이름
    std::string savename;           // 결과 저장 파일 이름
    std::string stdcode_filename;   // 법정동코드(binary) 파일 이름

    int num_workers;
    worker_context* worker_ctx;

    std::atomic<bool> submit_done;  // queue에 더는 푸시할 메세지가 없으며 queue가 비어있음
    boost::lockfree::queue<api_msg*, boost::lockfree::capacity<4096>> api_msg_queue;    // api msg queue

public:

};


#endif