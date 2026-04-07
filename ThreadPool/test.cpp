#include <iostream>
#include <string>
#include "src/ThreadPool.hpp"


int main()
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    ThreadPool &pool = ThreadPool::GetInstance();

    pool.AddTask([&pool]
        {
            std::cout << "parent\n";

            // 再帰AddTask（Shutdown前なら通る）
            const bool ok = pool.AddTask([] { std::cout << "child\n"; });
            if(!ok)
                std::cout << "AddTask rejected (shutting down)\n";
        });
	pool.AddTask([] { std::cout << "another task\n"; });
	pool.AddTask([] { std::cout << "another task2\n"; });

    // 受付停止→全タスク完了待ち→終了
    pool.Shutdown(ThreadPool::ShutdownMode::Drain);

	pool.AddTask([] { std::cout << "task after shutdown\n"; }); // 受付停止後は拒否される

    pool.WaitUntilIdle();

    SingletonController::Release();
    return 0;
}