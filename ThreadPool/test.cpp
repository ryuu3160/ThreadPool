#include <iostream>
#include <string>
#include "src/ThreadPool.hpp"

void SuperLongTask()
{
    std::cout << "super long task start\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "super long task end\n";
}

void LongTask()
{
    std::cout << "long task start\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "long task end\n";
}

class MyClass
{
public:
    void ClassTask()
    {
        std::cout << "class task\n";
    }
};


int main()
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    ThreadPool &pool = ThreadPool::GetInstance();
    MyClass obj;
	
    pool.AppendTask(&MyClass::ClassTask,&obj); // メンバ関数もOK

    pool.AppendTask([&pool]
        {
            std::cout << "parent\n";

            // 再帰AppendTask（Shutdown前なら通る）
            auto ok = pool.AppendTask([] { std::cout << "child\n"; });
            if(!ok)
                std::cout << "AppendTask rejected (shutting down)\n";
        });
	pool.AppendTask([] { std::cout << "another task\n"; });
    auto h = pool.AppendTask(LongTask);

    if(h)
    {
        // ポーリング
        while(!h->IsDone())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // あるいは待つ
        //h->Wait();

		std::cout << "task done" << std::endl;
    }
	pool.AppendTask(SuperLongTask);

    // 受付停止→全タスク完了待ち→終了
    pool.Shutdown(ThreadPool::ShutdownMode::Drain);

	pool.AppendTask([] { std::cout << "task after shutdown\n"; }); // 受付停止後は拒否される

    SingletonController::Release();
    return 0;
}