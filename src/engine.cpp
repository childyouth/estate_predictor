#include "engine.hpp"

using namespace boost::property_tree;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

std::string API_RENT_KEY;
std::string API_TRADE_KEY;

// Mutex for thread-safe output
std::mutex output_mutex;




void engine::allocate_task(ldcd_t lawd_cd){
    #pragma omp parallel for
    for(int i = 0;i < year_month_list_len; i++){
        ym_t deal_ymd = year_month_list[i];
        api_msg* rent_msg = new api_msg{api_type::RENT, lawd_cd, deal_ymd}; 
        api_msg* trade_msg = new api_msg{api_type::TRADE, lawd_cd, deal_ymd};
        
        push_task(rent_msg);
        push_task(trade_msg);
    }
}

void engine::allocate_work(){

    {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[" << std::string(20,'|') << "]" << "\n";
        std::cout << "[";
    }
    for (int i = 0;i<lawd_cd_list_len;i++){
        allocate_task(lawd_cd_list[i]);
        if(i % (lawd_cd_list_len / 20) == 0){
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "|";
        }
    }
    while(!api_msg_queue.empty()){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "|]" << "\n";
    }
    submit_done.store(true);

    std::cout << "== all work allocated ==" << "\n";
}

void engine::init_worker_ctx(int id){
    worker_ctx[id].id = id;
    worker_ctx[id].is_done = false;
    worker_ctx[id].num_working_tasks = 0;
    worker_ctx[id].num_finished_tasks = 0;
    worker_ctx[id].num_failed_tasks = 0;
    worker_ctx[id].error_tasks.clear();
}


void engine::push_task(api_msg* api_msg_ptr){
    while(!api_msg_queue.push(api_msg_ptr));
}

bool engine::pull_task(api_msg*& api_msg_ptr){
    return api_msg_queue.pop(api_msg_ptr);
}

bool engine::is_worker_busy(worker_context* ctx){
    return !(ctx->num_working_tasks < worker_max_workload);
}

bool engine::is_worker_busy(int thread_id){
    return is_worker_busy(&worker_ctx[thread_id]);
}

bool engine::is_worker_working(worker_context* ctx){
    return ctx->num_working_tasks;
}

bool engine::is_worker_working(int thread_id){
    return is_worker_working(&worker_ctx[thread_id]);
}

boost::asio::awaitable<void> engine::handle_api_msg(api_msg *msg, worker_context *worker_ctx)
{
    co_await asio::steady_timer(co_await asio::this_coro::executor, std::chrono::milliseconds(1)).async_wait(asio::use_awaitable);
    
    worker_ctx->num_working_tasks--;
    worker_ctx->num_finished_tasks++;
    co_return;
}


int engine::worker(int thread_id){
    worker_context* ctx = &worker_ctx[thread_id];

    asio::io_context io_context;
    api_msg* msg = NULL;

    while(!ctx->is_done){
        // worker가 여력이 없다면
        if(is_worker_busy(ctx)){
            // 빈자리 생길 수 있도록 코루틴 수행
            io_context.run_one();
        }
        // worker가 여력이 있다면
        else{
            // msg queue에 처리할 task가 있다면
            if(pull_task(msg)){
                // 코루틴 생성
                ctx->num_working_tasks++;
                asio::co_spawn(io_context,handle_api_msg(msg,ctx),asio::detached);
            }
            // msg queue가 비었으며 채워질 일이 없다면
            else if(submit_done){
                io_context.run();
                ctx->is_done = true;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(output_mutex);
        if(ctx->num_working_tasks){
            std::cout << is_worker_busy(ctx) << "\n";
            std::cout << "THREAD " << ctx->id << " : WORKING TASKS LEFT (" << ctx->num_working_tasks << ")" << "\n";
        }
        else{
            std::cout << "THREAD " << ctx->id << " : NO WORKING TASKS LEFT" << "\n";
        }
        std::cout << "THREAD " << ctx->id << " : Finished (" << ctx->num_finished_tasks << ")" << "\n";
    }
}



void engine::run(){
    
    asio::thread_pool pool(num_workers);

    for (int i = 0;i<num_workers;i++) {
        init_worker_ctx(i);

        asio::post(pool, [this, i]() {
            this->worker(i);
        });
    }

    // api_msg msg{api_type::RENT, 1,1};
    // push_task(&msg);
    // api_msg msg2{api_type::TRADE, 1,1};
    // push_task(&msg2);
    // submit_done.store(true);
    allocate_work();

    pool.join();

    int num_finished = 0;
    int num_failed = 0;
    int num_working = 0;
    for(int i = 0;i<num_workers;i++){
        num_working += worker_ctx[i].num_working_tasks;
        num_finished += worker_ctx[i].num_finished_tasks;
        num_failed += worker_ctx[i].num_failed_tasks;
    }
    std::cout << "STILL WORKING(should be 0) : (" << num_working << ")\n";
    std::cout << "TOTAL Finished : (" << num_finished << ")\n";
    std::cout << "TOTAL Failed : (" << num_failed << ")\n";
}



ym_t* engine::generate_year_month_list(){
    ym_t earliest_ymd = (ym_t)(std::stoi(ini_ptree.get<std::string>("variable.earliest_ymd","0")));
    ym_t latest_ymd = (ym_t)(std::stoi(ini_ptree.get<std::string>("variable.latest_ymd","0")));
    
    if (!(earliest_ymd && latest_ymd)){
        return NULL;
    }

    std::cout << "START_YM : " << earliest_ymd << "\n";
    std::cout << "END_YM : " << latest_ymd << "\n";

    ym_t e_y = earliest_ymd / 100;
    ym_t l_y = latest_ymd / 100;
    ym_t e_m = earliest_ymd % 100;
    ym_t l_m = latest_ymd % 100;

    ym_t total_month = (l_y - e_y) * 12 + (l_m - e_m) +1;
    ym_t index = 0;

    ym_t* ym_range = (ym_t*)malloc(sizeof(ym_t) * total_month);

    #pragma omp parallel
    {
        ym_t local_ym[total_month];
        ym_t local_index = 0;

        #pragma omp for
        for (ym_t year = e_y; year <= l_y; year++) {
            for (ym_t month = 1; month <= 12; month++) {
                ym_t date = year * 100 + month;
                if (date < earliest_ymd || date > latest_ymd) continue;
                local_ym[local_index++] = date;
            }
        }

        #pragma omp critical
        {
            for (int i = 0; i < local_index; ++i) {
                ym_range[index++] = local_ym[i];
            }
        }
    }

    year_month_list_len = total_month;
    std::cout << "TOTAL_MONTH : " << year_month_list_len << "\n";
    return ym_range;
}

ldcd_t* engine::parse_lawd_cd_list(){
    std::ifstream stdcode_file(stdcode_filename, std::ios::binary);
    if (!stdcode_file.is_open()){
        return NULL;
    }
    
    stdcode_file.seekg(0,std::ios::end);
    size_t file_size = stdcode_file.tellg();
    stdcode_file.seekg(0,std::ios::beg);

    lawd_cd_list_len = file_size / sizeof(ldcd_t);
    std::cout << "TOTAL_LAWD_CD : " << lawd_cd_list_len << "\n";
    ldcd_t* ldcd_list = (ldcd_t*)malloc(lawd_cd_list_len * sizeof(ldcd_t));

    stdcode_file.read((char*)ldcd_list, file_size);

    return ldcd_list;
}

void engine::parse_api_info()
{
    std::string base_addr = ini_ptree.get<std::string>("api_addr.api_base_addr");
    std::string rent_addr = ini_ptree.get<std::string>("api_addr.api_rent_endpoint");
    std::string trade_addr = ini_ptree.get<std::string>("api_addr.api_trade_endpoint");
    
    std::string rent_key = ini_ptree.get<std::string>("key.api_rent_key");
    std::string trade_key = ini_ptree.get<std::string>("key.api_trade_key");

    api_info.api_addr_and_key.resize((size_t)api_type::NUM_API_TYPE);

    api_info.api_addr_and_key[(int)api_type::RENT] = std::tuple<std::string, std::string, std::string>(base_addr, rent_addr, rent_key);
    api_info.api_addr_and_key[(int)api_type::TRADE] = std::tuple<std::string, std::string, std::string>(base_addr, trade_addr, trade_key);

    std::cout << "BASE ADDR : " << base_addr << "\n";
    std::cout << "ENDPOINT(RENT) : " << rent_addr << "\n";
    std::cout << "ENDPOINT(TRADE) : " << trade_addr << "\n";
    std::cout << "API_KEY (RENT) : " << rent_key << "\n";
    std::cout << "API_KEY (TRADE) : " << trade_key << "\n";
}

void engine::parse_app_args()
{
    stdcode_filename = ini_ptree.get<std::string>("engine.stdcode_filename");
    savename = ini_ptree.get<std::string>("engine.savename");
    num_workers = std::stoi(ini_ptree.get<std::string>("engine.num_thread"));
    worker_max_workload = std::stoi(ini_ptree.get<std::string>("engine.worker_max_workload"));

    std::cout << std::string(40, '=') << "\n";
    std::cout << "STDCODE FILENAME : " << stdcode_filename << "\n";
    std::cout << "SAVENAME : " << savename << "\n";
    std::cout << "NUM THREAD : " << num_workers << "\n";
    std::cout << "WORKER MAX WORKLOAD : " << worker_max_workload << "\n";
    std::cout << std::string(40, '=') << "\n";
}


engine::engine(std::string _ini_filename):ini_filename(_ini_filename), submit_done(false){
    try
    {
        read_ini(ini_filename, ini_ptree);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        exit(-1);
    }

    parse_app_args();
    parse_api_info();

    year_month_list = generate_year_month_list();
    if(!year_month_list){
        std::cerr << "ymlist" << "\n";
        exit(-1);
    }

    lawd_cd_list = parse_lawd_cd_list();
    if(!lawd_cd_list){
        std::cerr << "lawdlist" << "\n";
        exit(-1);
    }
    
    worker_ctx = (worker_context*)malloc(sizeof(worker_context) * num_workers);
}


int main() {
    
    engine engine("./estate_parser.ini");
    engine.run();

    // vector<tuple<string, string, map<string,string>>> api_requests = {
    //     {"apis.data.go.kr", "/1613000/RTMSDataSvcAptTrade/getRTMSDataSvcAptTrade", {{"LAWD_CD", "11110"}, {"DEAL_YMD", "202304"}, {"numOfRows", "1000"}}},
    //     {"apis.data.go.kr", "/1613000/RTMSDataSvcAptTrade/getRTMSDataSvcAptTrade", {{"LAWD_CD", "11110"}, {"DEAL_YMD", "202305"}, {"numOfRows", "1000"}}},
    //     {"apis.data.go.kr", "/1613000/RTMSDataSvcAptTrade/getRTMSDataSvcAptTrade", {{"LAWD_CD", "11110"}, {"DEAL_YMD", "202306"}, {"numOfRows", "1000"}}},
    //     {"apis.data.go.kr", "/1613000/RTMSDataSvcAptTrade/getRTMSDataSvcAptTrade", {{"LAWD_CD", "11110"}, {"DEAL_YMD", "202307"}, {"numOfRows", "1000"}}}
    // };

    return 0;
}